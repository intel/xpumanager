
install(
    FILES 
		DGM.py
		DGMCore.py
        README.md
        requirements.txt
    DESTINATION "${XPUM_INSTALL_DIR}/py"
)

install(
    FILES 
		client/DGMClient.py
		client/dgmcli
    DESTINATION "${XPUM_INSTALL_DIR}/py/client"
)