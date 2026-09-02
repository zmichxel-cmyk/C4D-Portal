// Shared mapping so the on-screen preview (PreviewPanel, CSS/canvas
// `filter`) and what's actually pushed to the virtual camera
// (webrtcReceiver's canvas draw for WiFi, electron-main.cjs's manual BGRA
// math for USB) apply the exact same adjustment for a given slider value.
//
// Sliders are 0-100 with 50 as the mockup's "neutral-ish" default; CSS
// filter functions (and the equivalent multiplicative math on the USB
// side) use 100% as neutral, so map value*2 -> percent: 50 -> 100%
// (unchanged), 0 -> 0% (e.g. black), 100 -> 200% (double).
export function sliderToPercent(value: number): number {
  return value * 2;
}

export function buildCssFilter(brightness: number, contrast: number, saturation: number): string {
  return `brightness(${sliderToPercent(brightness)}%) contrast(${sliderToPercent(contrast)}%) saturate(${sliderToPercent(saturation)}%)`;
}
