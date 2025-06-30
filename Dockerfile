

FROM python:3.10-alpine


COPY web/build /app/web/build
COPY espadmin /app/espadmin


RUN pip install Flask==3.0.3 Flask-Cors==4.0.1 flask-sock==0.7.0
WORKDIR /app

EXPOSE 9000/tcp

# CMD ["python", "./espadmin.py", "arg1"]
CMD ["python", "./espadmin/espadmin.py"]

