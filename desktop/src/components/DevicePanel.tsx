import { DeviceInfo, ConnectionStatus } from '../types';

interface Props {
  device: DeviceInfo | null;
  status: ConnectionStatus;
}

export default function DevicePanel({ device, status }: Props) {
  return (
    <section className="panel">
      <h2 className="panel-title">DEVICE</h2>
      <div className="device-card">
        <span className="device-card__icon">📱</span>
        <div className="device-card__text">
          <span className="device-card__label">{device?.name ?? 'No device'}</span>
          <span className={`device-card__status device-card__status--${status}`}>
            {status === 'connected' ? 'Connected' : status === 'connecting' ? 'Connecting…' : 'Disconnected'}
          </span>
          <span className="device-card__sub">{device?.osVersion ?? '—'}</span>
        </div>
        {device && (
          <span className="battery-pill">
            🔋 {device.batteryPercent}%
          </span>
        )}
      </div>
    </section>
  );
}
