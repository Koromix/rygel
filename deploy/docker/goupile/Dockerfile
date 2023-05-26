FROM debian:11 as build
RUN apt-get -y update
RUN apt-get -y install git gcc g++ clang-13 lld-13

COPY . /build
WORKDIR /build
RUN ./bootstrap.sh
RUN LDFLAGS="-static-libstdc++ -static-libgcc" ./felix -pParanoid --host=,clang-13,lld-13 -e goupile

FROM gcr.io/distroless/base-debian11
COPY --from=build /build/bin/Paranoid/goupile /goupile
CMD ["/goupile"]
