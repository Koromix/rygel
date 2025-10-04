FROM rygel/debian12 AS build
ADD repo.tar repo
RUN repo/bootstrap.sh
COPY versions.txt repo/versions.txt
RUN repo/felix -C repo/FelixBuild.ini -pParanoid --version_file versions.txt --host=:clang-18:lld-18 heimdall
COPY isolate.sh isolate.sh
RUN ./isolate.sh repo/bin/Paranoid/heimdall app
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build empty config
COPY --from=build empty data
COPY --from=build empty tmp
COPY --from=build repo/src/heimdall/dist/docker/heimdall.ini config/heimdall.ini

FROM scratch
COPY --from=app . .
CMD ["app/heimdall", "-C", "/config/heimdall.ini", "--sandbox"]
