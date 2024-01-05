# Create domain

To create a new domain, run the following command:

```sh
/usr/lib/goupile/manage.py create <name> [-p <HTTP port>]
```

Don't forget to **securely store** the backup decryption key!

# Delete domain

Delete the corresponding INI file in `/etc/goupile/domains.d` and stop the service:

```sh
rm /etc/goupile/domains.d/<name>.ini
/usr/lib/goupile/manage.py sync
```
