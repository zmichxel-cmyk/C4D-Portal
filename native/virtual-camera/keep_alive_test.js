// Dev-only smoke test: registers the virtual camera and pushes an animated
// color-cycling test frame so you can confirm the whole pipeline (register
// -> start -> shared memory -> media source DLL -> consuming app) works,
// without needing the full Electron app or a phone. Run with plain `node`.
const cam = require('./build/Release/c4dportal_virtual_camera.node');
console.log('create:', cam.create('C4D Portal'));
console.log('start:', cam.start());
console.log('Pushing animated test frames — check OBS preview now.');

const width = 640;
const height = 360;
const frame = Buffer.alloc(width * height * 4);
let hue = 0;

function hslToRgb(h) {
  const s = 0.7, l = 0.5;
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = l - c / 2;
  let r = 0, g = 0, b = 0;
  if (h < 60) [r, g, b] = [c, x, 0];
  else if (h < 120) [r, g, b] = [x, c, 0];
  else if (h < 180) [r, g, b] = [0, c, x];
  else if (h < 240) [r, g, b] = [0, x, c];
  else if (h < 300) [r, g, b] = [x, 0, c];
  else [r, g, b] = [c, 0, x];
  return [Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255)];
}

setInterval(() => {
  const [r, g, b] = hslToRgb(hue);
  for (let i = 0; i < width * height; i++) {
    frame[i * 4 + 0] = b;
    frame[i * 4 + 1] = g;
    frame[i * 4 + 2] = r;
    frame[i * 4 + 3] = 255;
  }
  cam.pushFrame(frame, width, height);
  hue = (hue + 6) % 360;
}, 33);
