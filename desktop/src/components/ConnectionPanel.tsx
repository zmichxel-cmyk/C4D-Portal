import { ConnectionType, ConnectionStatus } from '../types';

interface Props {
  selected: ConnectionType;
  status: ConnectionStatus;
  wifiAddress: string;
  onSelect: (type: ConnectionType) => void;
}

export default function ConnectionPanel({ selected, status, wifiAddress, onSelect }: Props) {
  return (
    <section className="panel">
      <h2 className="panel-title">CONNECTION</h2>

      <button
        className={`connection-card ${selected === 'usb' ? 'connection-card--active' : ''}`}
        onClick={() => onSelect('usb')}
      >
        <span className="connection-card__icon">🔌</span>
        <span className="connection-card__text">
          <span className="connection-card__label">USB Connection</span>
          <span className="connection-card__sub">Android Device</span>
        </span>
        {selected === 'usb' && (
          <span className={`status-dot status-dot--${status}`} />
        )}
      </button>

      <button
        className={`connection-card ${selected === 'wifi' ? 'connection-card--active' : ''}`}
        onClick={() => onSelect('wifi')}
      >
        <span className="connection-card__icon">📶</span>
        <span className="connection-card__text">
          <span className="connection-card__label">WiFi Connection</span>
          <span className="connection-card__sub">{wifiAddress}</span>
        </span>
        {selected === 'wifi' && (
          <span className={`status-dot status-dot--${status}`} />
        )}
      </button>
    </section>
  );
}
