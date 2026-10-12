FROM python:3.13.16-slim-bookworm
WORKDIR /app
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
COPY requirements.txt ./
RUN pip install --no-cache-dir --requirement requirements.txt
COPY exporter.py test_exporter.py ./
RUN python -m unittest -v test_exporter
USER 10001:10001
EXPOSE 9200
HEALTHCHECK --interval=10s --timeout=3s --retries=3 CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:9200/metrics', timeout=2).read()"]
CMD ["python", "exporter.py"]
