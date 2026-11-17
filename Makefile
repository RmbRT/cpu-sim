all:
	rmbrt-build opengl wasm "src/#CPU Simulator" :rmbrt-font :rmbrt-image/read#rmbrt-image :rmbrt-ui

clean:
	rm -rf /tmp/.rmbrt-runtime