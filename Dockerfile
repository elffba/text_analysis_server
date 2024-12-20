# GCC ile çalışma ortamı oluştur
FROM gcc:latest

# Çalışma dizinini ayarla
WORKDIR /usr/src/app

# Tüm dosyaları kopyala
COPY . .

# Programı derle
RUN gcc -o server main.c levenshtein.c -pthread -lm

# Çalıştırılacak komut
CMD ["./server"]
