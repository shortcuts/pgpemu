import io
import unittest

from PIL import Image

import screenshot_wiki as sw

MARKER = (255, 0, 0)


def png(w, h, bands=()):
    img = Image.new("RGB", (w, h), (255, 255, 255))
    for y0, y1 in bands:
        for y in range(y0, y1):
            for x in range(w):
                img.putpixel((x, y), MARKER)
    out = io.BytesIO()
    img.save(out, "PNG")
    return out.getvalue()


class Process(unittest.TestCase):
    def test_output_size(self):
        for w, h in ((1080, 2400), (1080, 2340)):
            out = Image.open(io.BytesIO(sw.process(png(w, h))))
            self.assertEqual(out.size, (540, 955))

    def test_too_short_raises(self):
        with self.assertRaisesRegex(ValueError, "too short"):
            sw.process(png(1080, 1500))

    def test_status_and_tab_bar_removed(self):
        top = int(0.04 * 2400)
        bottom = int(0.10 * 2400)
        data = png(1080, 2400, bands=[(0, top), (2400 - bottom, 2400)])
        out = Image.open(io.BytesIO(sw.process(data)))
        self.assertNotIn(MARKER, [c for _, c in out.getcolors(maxcolors=1 << 20)])


class TapPoint(unittest.TestCase):
    def test_tab_centers(self):
        points = [sw.tab_tap_point(i, 1080, 2400) for i in range(4)]
        self.assertEqual([p[0] for p in points], [135, 405, 675, 945])
        self.assertEqual({p[1] for p in points}, {2232})


if __name__ == "__main__":
    unittest.main()
