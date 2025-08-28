# Pricing

## HDS Hosting

Service | Description | Price
---------- | ----------- | -----
**HDS project** (public institution or non-profit) | Host a single eCRF by us | €150/month
**HDS project** (Private entity) | Host of a eCRF by us | €300/month
**HDS package** | Host an entire domain (with unlimited projects) by us | Custom quote

## Design Assistance

Service | Description | Price
---------- | ----------- | -----
**Design assistance** | Support via tickets and emails for assistance with your eCRF development | €100/month
**eCRF creation** | Development of a complete eCRF by our team | Custom quote
**Features** | Add new features to Goupile for your needs | Custom quote

# HDS Details

## Environments and Servers

Our HDS servers are automatically deployed using Ansible scripts, which are executed by our hosting provider [GPLExpert](https://gplexpert.com/) (HDS subcontractor and service manager).

We use two deployment environments: a pre-production environment (which manages the subdomains \*.preprod.goupile.fr) and a production environment. The pre-production environment is identical to production and allows us to test our deployment scripts. It contains only test domains and data.

Each environment uses two servers:

- *Proxy server*, which filters connections via NGINX and allows us to quickly redirect requests (to another backend) in case of an issue.
- *Backend server*, which contains the Goupile services and databases. The Goupile servers are accessible behind a second NGINX service running on the backend server.

Communication between the proxy server and the backend server occurs over a secure channel (IPSec and TLS 1.2+). Exchanges between the two NGINX services are protected by server and client certificates signed by an internal certificate created at deployment time (and whose private key is deleted immediately).

## Disaster Recovery Plan

Server environments are fully configured by automated Ansible scripts and can be reproduced identically within minutes.

Data restoration after loss of the main server can be performed from VPS snapshots taken nightly and retained for 14 days, which can be quickly restored by GPLExpert.
