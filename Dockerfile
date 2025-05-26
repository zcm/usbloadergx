# Build: 
# DOCKER_BUILDKIT=1 docker build -o output .

# For release builds (without debug):
# DOCKER_BUILDKIT=1 docker build --build-arg USE=release -o output .

# for Windows, use 
# { "features": { "buildkit": true } }
# instead of the environment variable

FROM devkitpro/devkitppc:20240702 as usbloader
RUN apt-get update -y && \
    apt-get install -y xz-utils make git zip

RUN mkdir /projectroot

# Now we have a container that has the dev environment set up. 
# Copy current folder into container, then compile
COPY . /projectroot/

ARG USE=debug

RUN cd /projectroot && make clean && make -j$(nproc) USE=$USE dist

RUN cat /opt/devkitpro/devkitPPC/powerpc-eabi/include/sys/iosupport.h


# Copy the DOL and ELF out of the container
FROM scratch AS export-stage
COPY --from=usbloader /projectroot/boot.* / 
COPY --from=usbloader /projectroot/usbloader_gx /
COPY --from=usbloader /projectroot/usbloadergx_r*.zip /
