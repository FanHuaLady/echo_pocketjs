import { definePocketConfig } from "../../third_party/pocketjs/framework/src/config.ts";

const LOOP = "3600ms";

export default definePocketConfig({
  theme: {
    keyframes: {
      "demo-orbit": {
        from: { rotate: 0, scale: 0.9 },
        "50%": { rotate: 180, scale: 1.08 },
        to: { rotate: 360, scale: 0.9 },
      },
      "demo-dot": {
        from: { opacity: 0.35, scale: 0.8 },
        "50%": { opacity: 1, scale: 1.15 },
        to: { opacity: 0.35, scale: 0.8 },
      },
      "demo-bar-a": {
        from: { width: 36 },
        "50%": { width: 180 },
        to: { width: 72 },
      },
      "demo-bar-b": {
        from: { width: 120 },
        "50%": { width: 54 },
        to: { width: 156 },
      },
      "demo-bar-c": {
        from: { width: 72 },
        "50%": { width: 156 },
        to: { width: 96 },
      },
    },
    animation: {
      "demo-orbit": { value: "demo-orbit 3.6s ease-in-out infinite both", loop: LOOP },
      "demo-dot-a": { value: "demo-dot 1.2s ease-in-out 0s infinite both", loop: LOOP },
      "demo-dot-b": { value: "demo-dot 1.2s ease-in-out 0.2s infinite both", loop: LOOP },
      "demo-dot-c": { value: "demo-dot 1.2s ease-in-out 0.4s infinite both", loop: LOOP },
      "demo-bar-a": { value: "demo-bar-a 3.6s ease-in-out infinite both", loop: LOOP },
      "demo-bar-b": { value: "demo-bar-b 3.6s ease-in-out 0.2s infinite both", loop: LOOP },
      "demo-bar-c": { value: "demo-bar-c 3.6s ease-in-out 0.4s infinite both", loop: LOOP },
    },
  },
});
