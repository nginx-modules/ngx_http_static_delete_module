ngx_http_static_delete
-----------------------------------------------------

The ngx_http_static_delete module is used to delete a file
in `nginx`'s document root.

Synopsis
=====================

    location /DELETE {
        allow           192.168.100.101;
        deny            all;
        root            /usr/local/nginx/html;
        static_delete   $uri;
    }
    

Diretives
=========

* syntax: static_delete $value
* default: --
* context: location

The value is the relative path of the static file to be deleted. It is 
possible to use a text, variables and their combination. It would be
mapped to root directory to construct a absolute path, then retry to 
delete it.
