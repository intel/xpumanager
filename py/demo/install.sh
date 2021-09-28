if [ -d xpumvenv ]; then
    echo virtual env exists;
    exit 0;
fi

python3 -m venv xpumvenv

source ./xpumvenv/bin/activate

python -m pip install --no-cache-dir -r requirements.txt

deactivate