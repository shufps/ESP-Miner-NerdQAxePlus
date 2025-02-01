# Run locally in browser with hot-reload

## Basic usage

```bash
# start node docker container
docker run --rm -it -v $(pwd):/app -p 4200:4200 node:18 /bin/bash

cd /app
npm install
npm run build-normal
npm run start-locally
```

It will build and expose the compiled app on `http://localhost:4200`

## Proxy config

The proxy conf is used to redirect `/api` calls to the API of your testing devices, like:
```json
{
  "/api": {
    "target": "http://192.168.0.161",
    "secure": false
  }
}
```
