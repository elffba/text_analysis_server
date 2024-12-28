FROM ubuntu:20.04

RUN apt-get update && apt-get install -y gcc make libc-dev

WORKDIR /app
COPY . .


# Matematik kütüphanesini ekliyoruz (-lm)
RUN gcc -o text_analysis_server main.c -lpthread -lm

CMD ["./text_analysis_server"]
