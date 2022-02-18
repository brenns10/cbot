FROM alpine:3.14
COPY cbot.apk /
RUN apk add --allow-untrusted ./cbot.apk openjdk11-jre gcompat && rm ./cbot.apk
RUN adduser --disabled-password --uid 1002 cbot && \
    mkdir -p /var/run/signald && \
    chown cbot /var/run/signald
COPY signald /opt/signald
COPY run.sh /bin/
RUN chmod 755 /bin/run.sh

USER cbot
WORKDIR /home/cbot
CMD ["/bin/run.sh"]
