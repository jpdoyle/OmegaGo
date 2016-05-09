from flask import Flask, url_for, render_template
app = Flask(__name__)


@app.route('/')
def hello_world(name=None):
    hello = 'hello'
    #return render_template('static/index.html', name=name)
    #return url_for('static', filename='index.html')
    return render_template('index.html', name=name)


if __name__ == '__main__':
    app.debug = True
    app.use_debugger = True
    app.run()