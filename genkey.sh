#!/bin/sh

ssh-keygen -t rsa -m PEM -N '' -f chroot/etc/ssh/ssh_host_rsa_key
