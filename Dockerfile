FROM paigeadele/compose-certificate-roller:v1.1

ENV DEBIAN_FRONTEND noninteractive

RUN apt update && apt -y install clang lld python3-pip cmake coreutils irssi

RUN pip install conan

WORKDIR /mnt/clandestine

ADD . .

RUN conan profile detect --force

RUN conan install . --output-folder=build/ --build=missing -s compiler.cppstd=11

WORKDIR /mnt/clandestine/build

RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake

RUN make -j $(nproc) install

RUN apt -y remove clang lld python3-pip cmake

RUN apt -y clean autoclean

RUN apt -y autoremove --yes

RUN rm -rf /var/lib/{apt,dpkg,cache,log}/

WORKDIR / 
