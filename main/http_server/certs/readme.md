openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 365 \
  -subj "/C=US/ST=Example/L=Example/O=Example/CN=esp32.local"


