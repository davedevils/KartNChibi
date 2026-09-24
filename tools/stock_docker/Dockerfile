# stock KnC client under 32 bit wine in xvfb driven by the pilot dll through a tcp bridge
# build context is the tools folder see run ps1 and Dockerfile dockerignore

FROM debian:trixie-slim AS bridge
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc-mingw-w64-i686 \
 && rm -rf /var/lib/apt/lists/*
COPY stock_docker/pilot_bridge.c /src/pilot_bridge.c
RUN i686-w64-mingw32-gcc -O2 -static -o /src/pilot_bridge.exe /src/pilot_bridge.c -lws2_32

FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive

# wine takes its recommends since it loads freetype gl and alsa at run time
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y wine wine32:i386 libgl1:i386 libglx-mesa0:i386 libgl1-mesa-dri:i386 \
 && apt-get install -y --no-install-recommends \
      xvfb xauth x11-utils x11-apps imagemagick mesa-utils \
      fonts-liberation fonts-dejavu-core fontconfig \
      python3 socat procps psmisc cabextract unzip ca-certificates curl \
 && rm -rf /var/lib/apt/lists/*

# winetricks stays in the image for a native dll when a builtin one falls short
RUN curl -fsSL -o /usr/local/bin/winetricks https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks \
 && chmod +x /usr/local/bin/winetricks

ENV WINEPREFIX=/opt/knc/prefix \
    WINEARCH=win32 \
    WINEDEBUG=-all \
    DISPLAY=:99

# alsa null device so dsound opens a real endpoint with no sound card
COPY stock_docker/asoundrc /root/.asoundrc
COPY stock_docker/prefix.reg /opt/knc/prefix.reg
RUN sed -i 's/\r$//' /root/.asoundrc

# the prefix is baked here so a run starts in seconds
# native vcrun2022 so the VS 2022 pilot dll runs on the same C++ runtime as on Windows
RUN WINEDLLOVERRIDES="mscoree,mshtml=" xvfb-run -a sh -c 'wineboot -i && wine regedit /S Z:\\opt\\knc\\prefix.reg && wineserver -w && winetricks -q vcrun2022 && wineserver -w' \
 && test -f /opt/knc/prefix/system.reg \
 && test -f /opt/knc/prefix/drive_c/windows/system32/msvcp140.dll \
 && grep -q '"\*msvcp140"="native,builtin"' /opt/knc/prefix/user.reg \
 && rm -rf /root/.cache/winetricks

COPY --from=bridge /src/pilot_bridge.exe /opt/knc/pilot_bridge.exe
COPY launcher_login.py stock_drive.py /opt/knc/tools/
COPY stock_docker/entrypoint.sh stock_docker/scenario.py /opt/knc/stock_docker/

# a windows checkout may carry crlf line ends which bash refuses
RUN sed -i 's/\r$//' /opt/knc/stock_docker/entrypoint.sh /opt/knc/stock_docker/scenario.py \
 && chmod +x /opt/knc/stock_docker/entrypoint.sh

ENTRYPOINT ["/opt/knc/stock_docker/entrypoint.sh"]
CMD ["boot"]
