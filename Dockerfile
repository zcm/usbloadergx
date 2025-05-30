# Build: 
# DOCKER_BUILDKIT=1 docker build -o output .

# For release builds (without debug):
# DOCKER_BUILDKIT=1 docker build --build-arg USE=release -o output .

# for Windows, use 
# { "features": { "buildkit": true } }
# instead of the environment variable

FROM devkitpro/devkitppc:20230827 as usbloader

# Debian buster is no longer supported - switch to archive mirror
RUN sed -Ei 's/\<deb.debian.org\>/archive.debian.org/g' \
      /etc/apt/sources.list.d/buster-backports.list

RUN apt-get update -y && \
    apt-get install -y xz-utils make git zip

RUN mkdir /projectroot

# Now we have a container that has the dev environment set up. 
# Copy current folder into container, then compile
COPY . /projectroot/

RUN cd /projectroot && git submodule update --init

ARG USE=debug
ARG NPROC

RUN cd /projectroot && make clean && make -j$NPROC`[ -z "$NPROC" ] && nproc` USE=$USE dist


# Copy the DOL and ELF out of the container
FROM scratch AS export-stage
COPY --from=usbloader /projectroot/boot.* / 
COPY --from=usbloader /projectroot/usbloader_gx /
COPY --from=usbloader /projectroot/usbloadergx_r*.zip /
