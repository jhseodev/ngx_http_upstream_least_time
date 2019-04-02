## ngx_http_upstream_least_time

## Description

Load balancing to the fastest back-end server.

## Installation

```bash
$ wget 'http://nginx.org/download/nginx-1.15.10.tar.gz'
$ tar -xzvf nginx-1.15.10.tar.gz
$ cd nginx-1.15.10/
$ patch -p1 < /path/to/ngx_http_upstream_least_time/least_time.patch

$ ./configure --add-module=/path/to/ngx_http_upstream_least_time

$ make
$ make install
```

## Usage

```Nginx
http {

    ...

    upstream testserver {
        least_time base=response mode=average;
        ...
    }

    server {
        listen 80;

        location / {
            proxy_pass http://testserver;
        }
    }
}
```

## TODO

sharing least-time between worker processes.
