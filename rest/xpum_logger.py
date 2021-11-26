import logging
import logging.handlers

# create logger
logger = logging.getLogger('xpum_rest')
logger.setLevel(logging.DEBUG)

# create formatter
formatter = logging.Formatter('[%(asctime)s] [%(name)s] [%(levelname)s] - %(message)s')

# create console handler
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
ch.setFormatter(formatter)

# create timed rotating file handler 
# fh = logging.handlers.TimedRotatingFileHandler(filename='xpum_rest.log', when='d', backupCount=3)
# fh.setLevel(logging.DEBUG)
# fh.setFormatter(formatter)

# add handlers to logger
logger.addHandler(ch)
# logger.addHandler(fh)

# logging function wrappers
def info(msg, *args, **kwargs):
    logger.info(msg, *args, **kwargs)

def debug(msg, *args, **kwargs):
    logger.debug(msg, *args, **kwargs)

def warn(msg, *args, **kwargs):
    logger.warn(msg, *args, **kwargs)

def error(msg, *args, **kwargs):
    logger.error(msg, *args, **kwargs)