FROM alpine:3.18
COPY cbot.apk /
COPY stephen@brennan.io.rsa.pub /etc/apk/keys/
RUN apk add ./cbot.apk gcompat tzdata msmtp perl curl && \
    rm ./cbot.apk && \
    adduser --disabled-password --uid 1002 cbot && \
    mkdir /var/cores && chmod 777 /var/cores

USER cbot
WORKDIR /home/cbot
ENV TZ=US/Pacific
CMD ["cbot" "/home/cbot/config/cbot.cfg"]
