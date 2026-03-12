FROM python:3.11-slim

WORKDIR /app

# vosk needs these at runtime for its shared libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
        wget \
        unzip \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY main.py .

# The Vosk model is NOT baked into the image — it's mounted as a volume.
# This keeps the image small and lets you swap models without rebuilding.
# See docker-compose.yaml for the download command.

EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')"

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
