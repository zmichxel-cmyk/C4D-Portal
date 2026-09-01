interface Props {
  pairingCode: string;
}

export default function PairingPanel({ pairingCode }: Props) {
  return (
    <section className="panel pairing-panel">
      <span className="pairing-panel__icon">⭐</span>
      <div className="pairing-panel__text">
        <span className="pairing-panel__label">Get the C4D Portal Mobile App</span>
        <span className="pairing-panel__sub">Scan to pair, or enter code {pairingCode} in the app.</span>
      </div>
      <div className="pairing-panel__qr" aria-label="Pairing QR code placeholder">
        QR
      </div>
    </section>
  );
}
