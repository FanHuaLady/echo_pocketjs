import { createCanvas, loadImage } from "../third_party/pocketjs/node_modules/@napi-rs/canvas/index.js";
import { resolve } from "node:path";

const projectDir = resolve(import.meta.dir, "..");
const sourcePath = resolve(projectDir, "img/flower.jpg");
const outputPath = resolve(projectDir, "app/display_demo/flower.png");
const textureSize = 256;

const source = await Bun.file(sourcePath).arrayBuffer();
const image = await loadImage(source);
const canvas = createCanvas(textureSize, textureSize);
const context = canvas.getContext("2d");

context.imageSmoothingEnabled = true;
context.drawImage(image, 0, 0, textureSize, textureSize);
await Bun.write(outputPath, canvas.toBuffer("image/png"));

console.log(
  `asset: ${sourcePath} (${image.width}x${image.height}) -> ` +
    `${outputPath} (${textureSize}x${textureSize} PNG)`,
);
