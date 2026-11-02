# Runs once the Houdini interface is ready: start the local Claude bridge.
try:
    import iwb_houdini_bridge
    iwb_houdini_bridge.start()
except Exception as error:
    print("IWB bridge could not start:", error)
