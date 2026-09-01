import { useEffect, useRef } from 'react';
import { ConnectionStatus, StreamSettings, StreamStats } from '../types';

interface Props {
  status: ConnectionStatus;
  settings: StreamSettings;
  stats: StreamStats;
  isPaused: boolean;
  liveStream: MediaStream | null;
  usbFrame: Uint8Array | null;
  onTogglePause: () => void;
  onToggleConnection: () => void;
}

// Draws an animated placeholder test pattern until a real decoded frame
// source (WebRTC video element / native virtual-camera frame buffer) is
// wired up. Confirms the render pipeline + canvas sizing work end to end.
function useTestPattern(canvasRef: React.RefObject<HTMLCanvasElement>, active: boolean) {
  useEffect(() => {
    if (!active) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let raf = 0;
    let t = 0;

    const draw = () => {
      const { width, height } = canvas;
      const gradient = ctx.createLinearGradient(0, 0, width, height);
      const hue = (t * 20) % 360;
      gradient.addColorStop(0, `hsl(${hue}, 45%, 18%)`);
      gradient.addColorStop(1, `hsl(${(hue + 60) % 360}, 45%, 10%)`);
      ctx.fillStyle = gradient;
      ctx.fillRect(0, 0, width, height);

      ctx.fillStyle = 'rgba(255,255,255,0.08)';
      ctx.font = '600 20px system-ui';
      ctx.textAlign = 'center';
      ctx.fillText('C4D Portal — no live source connected (test pattern)', width / 2, height / 2);

      t += 0.01;
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [canvasRef, active]);
}

export default function PreviewPanel({ status, settings, stats, isPaused, liveStream, usbFrame, onTogglePause, onToggleConnection }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const videoRef = useRef<HTMLVideoElement>(null);
  const hasLiveSource = Boolean(liveStream) || Boolean(usbFrame);
  useTestPattern(canvasRef, status === 'connected' && !isPaused && !hasLiveSource);

  useEffect(() => {
    if (videoRef.current) {
      videoRef.current.srcObject = liveStream;
    }
  }, [liveStream]);

  // USB frames arrive as JPEG bytes over IPC (see electron-main.cjs "USB
  // transport") — decode and draw each one as it comes in. Flip is applied
  // here too so the preview matches what's actually pushed to the virtual
  // camera (the push path in electron-main.cjs applies the same flip to
  // the raw BGRA buffer before pushFrame).
  useEffect(() => {
    if (!usbFrame) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let cancelled = false;
    createImageBitmap(new Blob([usbFrame as BlobPart], { type: 'image/jpeg' }))
      .then((bitmap) => {
        if (cancelled) return;
        if (canvas.width !== bitmap.width || canvas.height !== bitmap.height) {
          canvas.width = bitmap.width;
          canvas.height = bitmap.height;
        }
        ctx.save();
        ctx.translate(settings.flipHorizontal ? canvas.width : 0, settings.flipVertical ? canvas.height : 0);
        ctx.scale(settings.flipHorizontal ? -1 : 1, settings.flipVertical ? -1 : 1);
        ctx.drawImage(bitmap, 0, 0);
        ctx.restore();
        bitmap.close();
      })
      .catch(() => {});
    return () => {
      cancelled = true;
    };
  }, [usbFrame, settings.flipHorizontal, settings.flipVertical]);

  return (
    <section className="preview-panel">
      <h2 className="panel-title">LIVE PREVIEW</h2>

      <div className="preview-frame">
        {status === 'connected' ? (
          <>
            {liveStream ? (
              <video
                ref={videoRef}
                autoPlay
                muted
                playsInline
                className="preview-canvas"
                style={{
                  transform: `scale(${settings.flipHorizontal ? -1 : 1}, ${settings.flipVertical ? -1 : 1})`,
                }}
              />
            ) : (
              <canvas ref={canvasRef} width={1280} height={720} className="preview-canvas" />
            )}
            <div className="preview-badge">
              {settings.resolution.split(' ')[0]} • {settings.fps} FPS
            </div>
          </>
        ) : (
          <div className="preview-empty">
            <span className="preview-empty__icon">📷</span>
            <span>No device connected</span>
          </div>
        )}
      </div>

      <div className="preview-controls">
        <button className="control-btn control-btn--record" title="Record" disabled={status !== 'connected'}>⏺</button>
        <button className="control-btn" title="Screenshot" disabled={status !== 'connected'}>📷</button>
        <button className="control-btn" title={isPaused ? 'Resume' : 'Pause'} onClick={onTogglePause} disabled={status !== 'connected'}>
          {isPaused ? '▶' : '⏸'}
        </button>
        <button className="control-btn" title="Audio" disabled={status !== 'connected'}>🔊</button>

        <div className="audio-meter">
          <span className="audio-meter__label">Audio Input</span>
          <div className="audio-meter__bars">
            {Array.from({ length: 32 }).map((_, i) => (
              <span key={i} className={`audio-meter__bar ${status === 'connected' && !isPaused && i < 22 ? 'audio-meter__bar--active' : ''} ${i > 26 ? 'audio-meter__bar--hot' : ''}`} />
            ))}
          </div>
          <span className="audio-meter__db">-12 dB</span>
        </div>

        <button className="control-btn" title="Audio settings">⚙</button>
      </div>

      <div className="stats-bar">
        <div className="stat">
          <span className="stat__label">Resolution</span>
          <span className="stat__value">{status === 'connected' ? stats.resolution : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">FPS</span>
          <span className="stat__value">{status === 'connected' ? `${stats.fps} FPS` : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">Latency</span>
          <span className="stat__value">{status === 'connected' ? `${stats.latencyMs} ms` : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">Codec</span>
          <span className="stat__value">{status === 'connected' ? stats.codec : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">Battery</span>
          <span className="stat__value stat__value--good">{status === 'connected' ? `${stats.batteryPercent}%` : '—'}</span>
        </div>
      </div>

      <button
        className={`disconnect-btn ${status === 'disconnected' ? 'disconnect-btn--connect' : ''}`}
        onClick={onToggleConnection}
        disabled={status === 'connecting'}
      >
        {status === 'connected' ? 'DISCONNECT DEVICE' : status === 'connecting' ? 'CONNECTING…' : 'CONNECT DEVICE'}
      </button>
    </section>
  );
}
