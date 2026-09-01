import { StreamSettings } from '../types';

interface Props {
  settings: StreamSettings;
  onChange: <K extends keyof StreamSettings>(key: K, value: StreamSettings[K]) => void;
}

function Select({ label, value, options, onChange }: { label: string; value: string; options: string[]; onChange: (v: string) => void }) {
  return (
    <label className="field">
      <span className="field__label">{label}</span>
      <select className="field__select" value={value} onChange={(e) => onChange(e.target.value)}>
        {options.map((o) => (
          <option key={o} value={o}>{o}</option>
        ))}
      </select>
    </label>
  );
}

function Slider({ label, value, onChange }: { label: string; value: number; onChange: (v: number) => void }) {
  return (
    <div className="field">
      <div className="field__slider-row">
        <span className="field__label">{label}</span>
        <span className="field__slider-value">{value}%</span>
      </div>
      <input
        type="range"
        min={0}
        max={100}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
        className="field__slider"
      />
    </div>
  );
}

function Toggle({ label, checked, onChange }: { label: string; checked: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="field field--row">
      <span className="field__label">{label}</span>
      <button
        type="button"
        className={`toggle ${checked ? 'toggle--on' : ''}`}
        onClick={() => onChange(!checked)}
        aria-pressed={checked}
      >
        <span className="toggle__thumb" />
      </button>
    </label>
  );
}

export default function SettingsPanel({ settings, onChange }: Props) {
  return (
    <aside className="settings-panel">
      <h2 className="panel-title">SETTINGS</h2>

      <Select label="CAMERA" value={settings.cameraFacing === 'rear' ? 'Rear' : 'Front'}
        onChange={(v) => onChange('cameraFacing', v === 'Rear' ? 'rear' : 'front')}
        options={['Rear', 'Front']} />
      <Select label="RESOLUTION" value={settings.resolution} onChange={(v) => onChange('resolution', v)}
        options={['3840 x 2160 (4K)', '1920 x 1080 (FHD)', '1280 x 720 (HD)', '640 x 480 (SD)']} />
      <Select label="FPS" value={String(settings.fps)} onChange={(v) => onChange('fps', Number(v))}
        options={['24', '30', '60']} />
      <Select label="VIDEO FORMAT" value={settings.videoFormat} onChange={(v) => onChange('videoFormat', v)}
        options={['MJPEG', 'H.264', 'YUV420']} />
      <Select label="COLOR SPACE" value={settings.colorSpace} onChange={(v) => onChange('colorSpace', v)}
        options={['RGB', 'YUV']} />

      <h3 className="panel-subtitle">IMAGE ADJUSTMENTS</h3>
      <Slider label="Brightness" value={settings.brightness} onChange={(v) => onChange('brightness', v)} />
      <Slider label="Contrast" value={settings.contrast} onChange={(v) => onChange('contrast', v)} />
      <Slider label="Saturation" value={settings.saturation} onChange={(v) => onChange('saturation', v)} />
      <Slider label="Sharpness" value={settings.sharpness} onChange={(v) => onChange('sharpness', v)} />

      <h3 className="panel-subtitle">WHITE BALANCE</h3>
      <Select label="Mode" value={settings.whiteBalanceMode} onChange={(v) => onChange('whiteBalanceMode', v)}
        options={['Auto', 'Daylight', 'Cloudy', 'Fluorescent', 'Incandescent', 'Manual']} />

      <h3 className="panel-subtitle">ADVANCED</h3>
      <Toggle label="Low Latency Mode" checked={settings.lowLatencyMode} onChange={(v) => onChange('lowLatencyMode', v)} />
      <Toggle label="Mirror Video" checked={settings.mirrorVideo} onChange={(v) => onChange('mirrorVideo', v)} />
      <Select label="Anti-Flicker" value={settings.antiFlicker} onChange={(v) => onChange('antiFlicker', v)}
        options={['Off', '50Hz', '60Hz']} />
    </aside>
  );
}
