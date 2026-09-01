interface Props {
  onOpenSettings: () => void;
  onOpenFilters: () => void;
  onOpenAbout: () => void;
}

export default function ToolsPanel({ onOpenSettings, onOpenFilters, onOpenAbout }: Props) {
  return (
    <section className="panel">
      <h2 className="panel-title">TOOLS</h2>
      <button className="tool-row" onClick={onOpenSettings}>
        <span>⚙️</span> Settings
      </button>
      <button className="tool-row" onClick={onOpenFilters}>
        <span>🎛️</span> Filters
      </button>
      <button className="tool-row" onClick={onOpenAbout}>
        <span>ℹ️</span> About
      </button>
    </section>
  );
}
