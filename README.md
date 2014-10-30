## Installation

    cd nginx
    ./configure .... --add-module=../ngx_modules/ngx_http_prelog_module
    make

## Directive
    
    Syntax:     prelog path;
                prelog path [format];
                prelog off;
    Default:    off;
    Context:    http, server, location

The directive solve the problem: when upstream block, nginx can't log into
access_log, which make it hard to say who was the bad guy. prelog can log any
request before dive into upstream. In another word, it log into `path` when a
request go through the and of NGX_ACCESS_PHASE. Keep in mind that it only
logging for the first location, if internal redirect existed, it not logging.

If format not given, it will use `combined`.

## Example

    server {
        prelog /var/log/nginx/prelog.log main;

        location /test {
            prelog /var/log/nginx/prelog.log main;
        }
        
        location / {
            prelog /var/log/nginx/prelog.log;
        }

    }
