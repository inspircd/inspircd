%mode 0750
#!/bin/sh
set -e

# IMPORTANT: This script is an example post-deploy hook for use with CertBot,
# Dehydrated, or a similar SSL (TLS) renewal tool. You will need to customise
# it for your setup before you use it

# The location your renewal tool places your certificates.
CERT_DIR="/etc/letsencrypt/live/irc.example.com"

# The location of the InspIRCd config directory.
INSPIRCD_CONFIG_DIR="@CONFIG_DIR@"

# The location of the InspIRCd pid file.
INSPIRCD_PID_FILE="@RUNTIME_DIR@/inspircd.pid"

# The user:group that InspIRCd runs as.
INSPIRCD_OWNER="@USER@:@GROUP@"

if [ -e ${CERT_DIR} -a -e ${INSPIRCD_CONFIG_DIR} ]
then
    cp "${CERT_DIR}/fullchain.pem" "${INSPIRCD_CONFIG_DIR}/cert.pem"
    cp "${CERT_DIR}/privkey.pem" "${INSPIRCD_CONFIG_DIR}/key.pem"
    chown ${INSPIRCD_OWNER} "${INSPIRCD_CONFIG_DIR}/cert.pem" "${INSPIRCD_CONFIG_DIR}/key.pem"

    if [ -r ${INSPIRCD_PID_FILE} ]
    then
        kill -USR1 $(cat ${INSPIRCD_PID_FILE})
    elif [ -d /lib/systemd ] && systemctl --quiet is-active inspircd
    then
        systemctl kill --signal USR1 inspircd
    fi
fi
