# Create domain

To create a new domain, run the following command:

```sh
/usr/lib/goupile/create_domain <name> [-p <HTTP port>]
systemctl start goupile@<name>
```

Don't forget to **securely store** the backup decryption key!

# Delete domain

Delete the corresponding INI file in `/etc/goupile/domains.d` and stop the service:

```sh
rm /etc/goupile/domains.d/<name>.ini
systemctl stop goupile@<name>
```
