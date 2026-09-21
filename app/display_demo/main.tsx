import { mount } from "@pocketjs/framework";
import { Image, Text, View } from "@pocketjs/framework/components";
import { touches } from "@pocketjs/framework/input";
import { onFrame } from "@pocketjs/framework/lifecycle";
import { createSignal } from "solid-js";

const VIEWPORT_TOP = 60;
const VIEWPORT_H = 168;
const CONTENT_H = 336;
const MAX_SCROLL = CONTENT_H - VIEWPORT_H;
const SCROLLBAR_H = 84;
const SLIDER_X = 116;
const SLIDER_Y = 86;
const SLIDER_W = 150;
const SLIDER_THUMB = 12;

function clamp(value: number, minimum: number, maximum: number) {
  return Math.max(minimum, Math.min(maximum, value));
}

function Switch() {
  const [enabled, setEnabled] = createSignal(true);

  const trackClass = () =>
    enabled()
      ? "w-[48] h-[24] rounded-[12] bg-[#2f9e44] border-[1] border-[#6bd98a] flex-row items-center p-[3]"
      : "w-[48] h-[24] rounded-[12] bg-[#2c3f55] border-[1] border-[#48627d] flex-row items-center p-[3]";

  const knobClass = () =>
    enabled()
      ? "w-[18] h-[18] rounded-full bg-[#f4f7fb] translate-x-[24]"
      : "w-[18] h-[18] rounded-full bg-[#f4f7fb]";

  return (
    <View
      class={trackClass()}
      focusable
      onPress={() => setEnabled(!enabled())}
    >
      <View class={knobClass()} />
    </View>
  );
}

// App 是一个普通函数，但它的返回值不是字符串，而是 JSX
// PocketJS 界面 = 一个返回 JSX 的函数
function App() {
  const [tapped, setTapped] = createSignal(false);
  const [wifi, setWifi] = createSignal(false);
  const [mode, setMode] = createSignal(0);
  const [level, setLevel] = createSignal(50);
  const [scrollY, setScrollY] = createSignal(0);
  const [fps, setFps] = createSignal(0);
  const [touchText, setTouchText] = createSignal("UP");
  let dragging = false;
  let lastY = 0;

  (globalThis as unknown as {
    __rv1106HostStats?: (opcode: number, value: number) => number;
  }).__rv1106HostStats = (opcode: number, value: number) => {
    if (opcode === 1) setFps(value);
    return 0;
  };

  onFrame(() => {
    const touch = touches()[0];

    if (!touch) {
      setTouchText("UP");
      dragging = false;
      return;
    }

    setTouchText(`${touch.x},${touch.y}`);
    if (!dragging && touch.y >= VIEWPORT_TOP && touch.y <= VIEWPORT_TOP + VIEWPORT_H) {
      dragging = true;
      lastY = touch.y;
    }
    if (dragging) {
      const dy = touch.y - lastY;
      if (dy !== 0) setScrollY((value) => clamp(value - dy, 0, MAX_SCROLL));
      lastY = touch.y;
    }

    const contentY = touch.y - VIEWPORT_TOP + scrollY();
    if (contentY >= SLIDER_Y - 10 && contentY <= SLIDER_Y + 36 && touch.x >= SLIDER_X - 10 && touch.x <= SLIDER_X + SLIDER_W + 10) {
      setLevel(Math.round(clamp((touch.x - SLIDER_X) / SLIDER_W, 0, 1) * 100));
    }
  });

  const wifiClass = () =>
    wifi()
      ? "w-[48] h-[24] rounded-[12] bg-[#146c94] border-[1] border-[#70d6ff] flex-row items-center p-[3]"
      : "w-[48] h-[24] rounded-[12] bg-[#2c3f55] border-[1] border-[#48627d] flex-row items-center p-[3]";
  const wifiKnobClass = () =>
    wifi()
      ? "w-[18] h-[18] rounded-full bg-[#f4f7fb] translate-x-[24]"
      : "w-[18] h-[18] rounded-full bg-[#f4f7fb]";
  const sliderFillWidth = () => (level() / 100) * SLIDER_W;
  const sliderThumbX = () => (level() / 100) * (SLIDER_W - SLIDER_THUMB);
  const scrollThumbY = () => (scrollY() / MAX_SCROLL) * (VIEWPORT_H - SCROLLBAR_H);
  const modeText = () => (mode() === 0 ? "MODE A" : mode() === 1 ? "MODE B" : "MODE C");

  return (
    <View class="w-full h-full flex-col p-[12] bg-gradient-to-b from-[#08111f] to-[#142942]">
      <View class="flex-row justify-between items-start">
        <View class="flex-col">
          <Text class="text-xs text-[#70d6ff] font-bold">FPS {fps()}</Text>
          <Text class="text-xl text-[#f4f7fb] font-bold tracking-wide mt-[2]">POCKETJS</Text>
        </View>
        <View class="w-[40] h-[40] rounded-full flex-row justify-center items-center bg-[#183b59] animate-demo-orbit">
          <View class="w-[18] h-[18] rounded-full bg-[#70d6ff]" />
        </View>
      </View>

      <View class="relative w-full h-[168] mt-[8] overflow-hidden">
        <View class="relative w-full" style={{ height: CONTENT_H, translateY: -scrollY() }}>
          <View class="absolute flex-row items-center gap-[12]" style={{ insetL: 0, insetT: 0, width: 296, height: 74 }}>
            <View class="w-[74] h-[74] p-[4] rounded-[10] bg-[#edf1f5] overflow-hidden shadow">
              <Image class="w-full h-full rounded-[6]" src="flower.png" />
            </View>

            <View class="flex-1 flex-col gap-[8]">
              <View class="flex-row items-center justify-between">
                <Text class="text-xs text-[#c7d8e8] font-bold">TOUCH</Text>
                <Text class="text-xs text-[#70d6ff] font-bold">{touchText()}</Text>
              </View>

              <View class="flex-row items-center justify-between">
                <Text class="text-xs text-[#c7d8e8] font-bold">SWITCH</Text>
                <Switch />
              </View>
            </View>
          </View>

          <View
            class="absolute rounded-[8] bg-[#102238] flex-row items-center px-[10]"
            style={{ insetL: 0, insetT: SLIDER_Y, width: 296, height: 36 }}
            focusable
            onPress={() => setLevel(level() >= 100 ? 0 : level() + 10)}
          >
            <Text class="w-[94] text-xs text-[#c7d8e8] font-bold">SLIDER {level()}</Text>
            <View class="relative w-[150] h-[6] rounded-[5] bg-[#263f5a]">
              <View class="h-full rounded-[5] bg-[#ffd166]" style={{ width: sliderFillWidth() }} />
              <View
                class="absolute left-0 top-[-3] w-[12] h-[12] rounded-full bg-[#f4f7fb] border-[1] border-[#ffd166]"
                style={{ translateX: sliderThumbX() }}
              />
            </View>
          </View>

          <View
            class={tapped()
              ? "absolute rounded-[8] border-[1] border-[#70d6ff] bg-[#2b6b91] flex-row justify-center items-center"
              : "absolute rounded-[8] border-[1] border-[#2c5a7c] bg-[#153653] flex-row justify-center items-center"}
            style={{ insetL: 0, insetT: 132, width: 296, height: 30 }}
            focusable
            onPress={() => setTapped(!tapped())}
          >
            <Text class="text-xs text-[#f4f7fb] font-bold tracking-wide">{tapped() ? "BUTTON ON" : "BUTTON OFF"}</Text>
          </View>

          <View class="absolute rounded-[8] bg-[#102238] flex-row items-center justify-between px-[10]" style={{ insetL: 0, insetT: 174, width: 296, height: 34 }}>
            <Text class="text-xs text-[#c7d8e8] font-bold">WIFI</Text>
            <View class={wifiClass()} focusable onPress={() => setWifi(!wifi())}>
              <View class={wifiKnobClass()} />
            </View>
          </View>

          <View
            class="absolute rounded-[8] border-[1] border-[#48627d] bg-[#153653] flex-row justify-center items-center"
            style={{ insetL: 0, insetT: 216, width: 296, height: 30 }}
            focusable
            onPress={() => setMode((mode() + 1) % 3)}
          >
            <Text class="text-xs text-[#f4f7fb] font-bold tracking-wide">{modeText()}</Text>
          </View>

          <View class="absolute p-[10] rounded-[10] bg-[#102238] shadow" style={{ insetL: 0, insetT: 258, width: 296, height: 50 }}>
            <View class="flex-row justify-end items-center">
              <View class="flex-row gap-[5]">
                <View class="w-[8] h-[8] rounded-full bg-[#ff6b6b] animate-demo-dot-a" />
                <View class="w-[8] h-[8] rounded-full bg-[#ffd166] animate-demo-dot-b" />
                <View class="w-[8] h-[8] rounded-full bg-[#51cf66] animate-demo-dot-c" />
              </View>
            </View>
            <View class="flex-col gap-[5] mt-[7]">
              <View class="w-full h-[6] rounded-[3] bg-[#1d3853]">
                <View class="h-full rounded-[3] bg-[#ff6b6b] animate-demo-bar-a" />
              </View>
              <View class="w-full h-[6] rounded-[3] bg-[#1d3853]">
                <View class="h-full rounded-[3] bg-[#ffd166] animate-demo-bar-b" />
              </View>
              <View class="w-full h-[6] rounded-[3] bg-[#1d3853]">
                <View class="h-full rounded-[3] bg-[#51cf66] animate-demo-bar-c" />
              </View>
            </View>
          </View>
        </View>

        <View class="absolute right-[0] top-[0] w-[4] h-full rounded-[2] bg-[#1d3853]">
          <View
            class="w-full rounded-[2] bg-[#70d6ff]"
            style={{ height: SCROLLBAR_H, translateY: scrollThumbY() }}
          />
        </View>
      </View>
    </View>
  );
}

// 调用时返回 App 组件
mount(() => <App />);
