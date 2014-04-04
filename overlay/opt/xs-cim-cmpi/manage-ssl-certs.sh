#!/bin/bash

####### LOAD XENSOURCE-INVENTORY #######

. /etc/xensource-inventory


########## GLOBAL VARIABLES ############

PEG_HOME="/opt/openpegasus"
SYM_CERT=$PEG_HOME"/ssl-cert.pem"
XAPI_CERT="/etc/xensource/xapi-ssl.pem"
CIMCONFIG=$PEG_HOME"/bin/cimconfig"
PEMFILE=$PEG_HOME"/self-signed.pem"

CERT_CONFIG="sslCertificateFilePath"
KEY_CONFIG="sslKeyFilePath"

############ GENERATE A SELF SIGNED CERTIFICATE ###############
function generate_self_signed_certificate {

    XEN_HOSTNAME=$(hostname)

    keyfile=$PEG_HOME"/self-signed.key"
    certfile=$PEG_HOME"/self-signed.cert"

    openssl genrsa 1024 >"$keyfile"
    openssl req -new -x509 -nodes -days 365 -key "$keyfile" > "$certfile" <<EOF
UK
Cambridgeshire
Cambridge
Citrix
.
$XEN_HOSTNAME
.
EOF

    cat "$keyfile" "$certfile" > "$PEMFILE"
    rm "$keyfile" "$certfile"

}

############# LINK TO THE NEW CERTIFICATE ###################
function link_sym_cert() {
    cert="$1"
    if [ -e $SYM_CERT ]; then
       rm $SYM_CERT
    fi

    ln -s "$cert" "$SYM_CERT"
    echo $cert
    ############# SET THE SYM CERT IN OPENPEG ###############
    $CIMCONFIG -s $CERT_CONFIG=$SYM_CERT -p
    $CIMCONFIG -s $KEY_CONFIG=$SYM_CERT -p
}

############# REMOVE CN OTHER_CONFIG CACHE ##################

function remove_cn_other_config_cache() {
    echo "Attempt to remove CN if in other_config..."
    xe host-param-remove uuid=$INSTALLATION_UUID param-name=other-config param-key=host_cn

}

############# RESTART OPENPEGASUS ###########################
function restart_pegasus() {
    cd $PEG_HOME
    ./bin/cimserver -s
    ./bin/cimserver
}
############## PARSE COMMAND ARGUMENTS ######################
case "$1" in
     use-xapi)
        echo "Using XAPI's cert..."
        link_sym_cert $XAPI_CERT
        restart_pegasus
        remove_cn_other_config_cache
        echo "Done"
        ;;

     generate)
        echo "Generating a self-signed cert..."
        generate_self_signed_certificate
        link_sym_cert $PEMFILE
        restart_pegasus
        remove_cn_other_config_cache
        ;;
     *)
        echo "Usage:"
	echo "Please specify the command line argument 'use-xapi' or 'generate'."
        exit 1
esac

