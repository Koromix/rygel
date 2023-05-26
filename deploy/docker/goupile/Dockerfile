FROM ubuntu:latest as build
RUN apt-get -y update
RUN apt-get -y install git gcc g++ clang-15 lld-15

COPY . /build
WORKDIR /build
RUN ./bootstrap.sh
RUN ./felix -pParanoid --host=,clang-15,lld-15 goupile

FROM gcr.io/distroless/base-debian11
COPY --from=build /build/bin/Paranoid/goupile /goupile
CMD ["/goupile"]
