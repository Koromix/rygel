FROM rygel/debian12 AS build
ADD repo.tar repo
COPY versions.txt repo/versions.txt
RUN repo/bootstrap.sh
RUN repo/felix -C repo/FelixBuild.ini -pParanoid --version_file versions.txt --host=:clang-18:lld-18 heimdall
COPY isolate.sh isolate.sh
RUN ./isolate.sh repo/bin/Paranoid/heimdall app
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build repo/src/heimdall/dist/docker/heimdall.ini app/heimdall.ini
COPY --from=build empty data
COPY --from=build empty tmp

FROM scratch
COPY --from=app . .
CMD ["app/heimdall", "-C", "/app/heimdall.ini", "--sandbox"]
