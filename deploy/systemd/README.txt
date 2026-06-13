Nexus Systems — systemd Unit Templates
======================================

These units manage the Nexus ecosystem as a set of Docker Compose
services under systemd supervision.

Files
-----
  nexus-cloud.service       Starts Nexus-Cloud (+ infra: postgres, redis, minio)
  nexus-app@.service        Template unit — start any app by name, e.g. nexus-graphic
  nexus-ecosystem.target    Grouping target — start/stop everything at once

Installation
------------
1. Adjust paths and user/group if your setup differs:

     # Edit each .service file and change:
     User=%U      →  User=deploy
     Group=%G     →  Group=deploy
     WorkingDirectory=%h/nexus-systems  →  WorkingDirectory=/opt/nexus-systems

     Or use environment override:
     $ sudo systemctl edit nexus-cloud.service
     [Service]
     User=deploy
     Group=deploy
     WorkingDirectory=/opt/nexus-systems

2. Copy units to systemd system directory:

     sudo cp deploy/systemd/nexus-cloud.service   /etc/systemd/system/
     sudo cp deploy/systemd/nexus-app@.service     /etc/systemd/system/
     sudo cp deploy/systemd/nexus-ecosystem.target /etc/systemd/system/

3. Reload systemd and enable:

     sudo systemctl daemon-reload
     sudo systemctl enable nexus-cloud.service
     sudo systemctl enable nexus-ecosystem.target

4. Start the cloud first, then apps:

     sudo systemctl start nexus-cloud.service
     sudo systemctl enable nexus-app@nexus-graphic.service --now
     sudo systemctl enable nexus-app@nexus-design.service  --now
     sudo systemctl enable nexus-app@nexus-draw.service    --now
     sudo systemctl enable nexus-app@nexus-photos.service  --now
     sudo systemctl enable nexus-app@nexus-video.service   --now

   Or start the entire ecosystem target (groups all services):

     sudo systemctl start nexus-ecosystem.target

Status and Logs
---------------
  systemctl status nexus-cloud
  systemctl status nexus-app@nexus-graphic
  systemctl status nexus-ecosystem.target

  journalctl -u nexus-cloud -f
  journalctl -u nexus-app@nexus-graphic -f

  # Follow all ecosystem services
  journalctl -u nexus-cloud -u 'nexus-app@*' -f

Stop / Disable
--------------
  sudo systemctl stop nexus-app@nexus-video
  sudo systemctl disable nexus-app@nexus-video

  sudo systemctl stop nexus-ecosystem.target    # stops everything
  sudo systemctl disable nexus-ecosystem.target # disables everything

Port Calculation for Template Instances
----------------------------------------
The %i parameter is the service name (e.g. nexus-graphic, nexus-photos).
Docker Compose handles port assignment per-service in docker-compose.yml.

Known service names for nexus-app@.service:
  nexus-graphic  (3080)
  nexus-design   (3074)
  nexus-draw     (3075)
  nexus-photos   (3096)
  nexus-video    (3113)

Uninstall
---------
  sudo systemctl stop nexus-ecosystem.target
  sudo systemctl disable nexus-ecosystem.target nexus-cloud.service
  sudo rm /etc/systemd/system/nexus-cloud.service
  sudo rm /etc/systemd/system/nexus-app@.service
  sudo rm /etc/systemd/system/nexus-ecosystem.target
  sudo systemctl daemon-reload
