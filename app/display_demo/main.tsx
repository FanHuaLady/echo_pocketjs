import { mount } from "@pocketjs/framework";
import { Image, Text, View } from "@pocketjs/framework/components";
import { createSignal } from "solid-js";

function App() {
  const [tapped, setTapped] = createSignal(false);

  return (
    <View class="w-full h-full flex-col p-[16] bg-gradient-to-b from-[#08111f] to-[#142942]">
      <View class="flex-row justify-between items-center">
        <Text class="text-xl text-[#f4f7fb] font-bold tracking-wide">POCKETJS</Text>
        <View class="w-[40] h-[40] rounded-full flex-row justify-center items-center bg-[#183b59] animate-demo-orbit">
          <View class="w-[18] h-[18] rounded-full bg-[#70d6ff]" />
        </View>
      </View>

      <View class="flex-1 flex-col items-center justify-center">
        <View class="w-[96] h-[96] p-[4] rounded-[10] bg-[#edf1f5] overflow-hidden shadow">
          <Image class="w-full h-full rounded-[6]" src="flower.png" />
        </View>

        <View
          class={tapped()
            ? "w-[132] h-[30] mt-[8] rounded-[8] border-[1] border-[#70d6ff] bg-[#2b6b91] flex-row justify-center items-center"
            : "w-[132] h-[30] mt-[8] rounded-[8] border-[1] border-[#2c5a7c] bg-[#153653] flex-row justify-center items-center"}
          focusable
          onPress={() => setTapped(!tapped())}
        >
          <Text class="text-xs text-[#f4f7fb] font-bold tracking-wide">TOUCH</Text>
        </View>

        <View class="w-full h-[60] mt-[10] p-[10] rounded-[10] bg-[#102238] shadow">
          <View class="flex-row justify-end items-center">
            <View class="flex-row gap-[5]">
              <View class="w-[8] h-[8] rounded-full bg-[#ff6b6b] animate-demo-dot-a" />
              <View class="w-[8] h-[8] rounded-full bg-[#ffd166] animate-demo-dot-b" />
              <View class="w-[8] h-[8] rounded-full bg-[#51cf66] animate-demo-dot-c" />
            </View>
          </View>
          <View class="flex-col gap-[6] mt-[8]">
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
    </View>
  );
}

mount(() => <App />);
