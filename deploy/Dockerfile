FROM alpine:3.14
COPY cbot.apk /
COPY run.sh /bin/
RUN apk add --allow-untrusted ./cbot.apk gcompat tzdata && \
    rm ./cbot.apk && \
    adduser --disabled-password --uid 1002 cbot && \
    chmod 755 /bin/run.sh && \
    mkdir /var/cores && chmod 777 /var/cores

USER cbot
WORKDIR /home/cbot
ENV TZ=US/Pacific
CMD ["/bin/run.sh"]
