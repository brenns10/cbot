#!/usr/bin/env python3
import code
import threading

from flask import Flask, jsonify
app = Flask(__name__)

price = 16500

@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def hello_world(path):
    # lol
    return jsonify({
        'data': {
            'BTC': [
                {
                    'quote': {
                        'USD': {
                            'price': price
                        }
                    }
                }
            ]
        }
    })


if __name__ == '__main__':
    threading.Thread(target=app.run, kwargs={"port": 4100}).start()
    code.interact(local=locals())
