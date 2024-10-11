### Run the setup

First you need to install Docker:

```
# Install Docker
sudo apt update && sudo apt install docker.io docker-compose -y

# Start Docker
sudo systemctl start docker && sudo systemctl enable docker
```


Before the setup is run, the data directories need to be created:

```
sudo ./create_data_directories.sh
```

Afterwards start with:
```
docker compose up -d
```

Then, Grafana should be available at `http://localhost:3000`.

Default Username and Password is `admin` and `foobar`

To stop the monitoring use:
```
docker compose down
```

