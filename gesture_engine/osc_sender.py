"""
OSC sender — wraps python-osc to emit /gesture/lanes bundles.
"""

from pythonosc import udp_client
from pythonosc.osc_bundle_builder import OscBundleBuilder
from pythonosc.osc_message_builder import OscMessageBuilder
import pythonosc.osc_bundle_builder as bundle_builder


class OscSender:
    def __init__(self, host="127.0.0.1", port=9000):
        self._client = udp_client.SimpleUDPClient(host, port)
        self.host = host
        self.port = port

    def send_lanes(self, values: list[float]):
        """
        Send 8 gesture feature floats as /gesture/lanes f f f f f f f f
        values: list of 8 floats in [0.0, 1.0]
        """
        builder = OscMessageBuilder(address="/gesture/lanes")
        for v in values:
            builder.add_arg(float(v))
        msg = builder.build()
        self._client.send(msg)
