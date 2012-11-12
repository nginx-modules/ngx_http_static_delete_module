# vi:filetype=perl

use Test::Nginx::Socket;

repeat_each(1);

no_shuffle();

plan tests => repeat_each() * blocks();

run_tests();

__DATA__

=== TEST 1: No such file 
--- config
        location ~ /DELETE/(.*) {
            allow           127.0.0.1;
            deny            all;
            root            /usr/local/nginx/html;
            static_delete   /$1;
        }
--- request
    GET /DELETE/no_suck_file
--- error_code: 404


=== TEST 2: success 
--- config
        location ~ /DELETE/(.*) {
            allow           127.0.0.1;
            deny            all;
            root            html;
            static_delete   /$1;
        }
--- user_files
>>> dummy.foo.bar
It's dummy from 'delete.t'.
--- request
    GET /DELETE/dummy.foo.bar
--- error_code: 200
