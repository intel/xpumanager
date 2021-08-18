
install(
    FILES 
		DGM.py
		DGMCore.py
        README.md
        requirements.txt
    DESTINATION 
      py
)

install(
    FILES 
		client/xpumcli
    DESTINATION 
      py/client
)