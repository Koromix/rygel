FROM --platform=$BUILDPLATFORM rygel/debian12 AS build
ADD repo.tar repo
RUN repo/bootstrap.sh
COPY versions.txt repo/versions.txt
ARG TARGETARCH
RUN CPPFLAGS="-DDEFAULT_CONFIG=/config/heimdall.ini -DDEFAULT_PORT=80" repo/felix -ve -C repo/FelixBuild.ini -pParanoid --version_file repo/versions.txt --host=$TARGETARCH:clang-18:lld-18 heimdall
COPY isolate.py isolate.py
RUN ./isolate.py -p /app -O app repo/bin/Paranoid/heimdall
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build empty data
COPY --from=build empty tmp

FROM scratch
COPY --from=app . .
CMD ["app/heimdall", "--sandbox"]
