# Overview

This is a fake keyboard driver that types whatever keys are written to `/dev/dev_kbd`. In particular, write lines to `/dev/dev_kbd` of the form `code,value`, where `value` is `1` for pressed and `0` for released. Here's an example of how to hit enter:

```shell
sudo bash -c '(echo 28,1; echo 28,0) >/dev/dev_kbd'
```