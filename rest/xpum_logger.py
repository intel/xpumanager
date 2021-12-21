import logging
import logging.handlers
from syslog import openlog as opensyslog, LOG_PID, LOG_CONS, LOG_USER, syslog

# create logger
app_logger = logging.getLogger('xpum_rest')
app_logger.setLevel(logging.DEBUG)

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
app_logger.addHandler(ch)
# logger.addHandler(fh)

# set up syslog
opensyslog('xpumrest', LOG_PID | LOG_CONS, LOG_USER)

# logging function wrappers
def trace(msg, *args, **kwargs):
    # level lower than DEBUG(10) and higher than NOTSET(0)
    app_logger.log(5, msg, *args, **kwargs)

def info(msg, *args, **kwargs):
    app_logger.info(msg, *args, **kwargs)

def debug(msg, *args, **kwargs):
    app_logger.debug(msg, *args, **kwargs)

def warn(msg, *args, **kwargs):
    app_logger.warn(msg, *args, **kwargs)

def error(msg, *args, **kwargs):
    app_logger.error(msg, *args, **kwargs)

def fatal(msg, *args, **kwargs):
    app_logger.critical(msg, *args, **kwargs)

def audit(user, remote_address, action, result_code, ext_fmt, *args, **kwargs):
    syslog(f'[{user}@{remote_address}] Action:{action}, Result:{result_code}, ExtraMsg: {ext_fmt.format(*args, **kwargs)}')


if __name__ == '__main__':
    info('logger started')
    audit('xpumadmin', '127.0.0.1', 'SayHello',0, 'hello {}', 'world!')