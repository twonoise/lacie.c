# lacie.c
lacie.c is Linux Ansi Console Image Editor for 1-bit or 4-bit palette, with transparency

If you are here, you probably noticed that today there is too few to no any tiny, preferably console based (which implies keyboard-driven), low colors, user friendly (include code), few dependency, yet rich pixel-precise editors with transparency, block copy, fine-tuned circles and undo.
Here I am try to somewhat fix this. So I write it myself.

As the code is tiny and self explaining (I hope), please learn all keyboard bindings from code. And, most probably, you would need to change some, as key codes are terminal dependent. Please feel free to update code and add your features.

The file support is read and write [`.pam` file](https://en.wikipedia.org/wiki/Netpbm) with: `DEPTH 4, MAXVAL 3, TUPLTYPE RGB_ALPHA`. Note that depth of 4 allows for transparency. Note that `DEPTH` here is channels (bytes per pixel) quantity, where 4 means ABGR format.

    magick test.png -depth 2 test.pam
    magick display test.pam   # Use middle button for no-blur zoom.
    lacie test.pam

Probably, some preprocessing will be need. With GIMP 2.x, using some small image:
* Check if transparent areas are displayed correctly.
* * If not (filled with some color): Use `Select by color` (Shift+O) or Tools → Selection Tools → Fuzzy Select (U), click on that areas, and use `Del`.
* * * Maybe, `Layer → transparency → Add alpha channel` will be need first.
* Maybe, `Increase contrast` will help to reduce number of colors, yet get pure MDA (1 bit) or EGA64 (6 bit) colors.
* Use `Color → Levels` and draw two (three) step ladder to use bi- or tri-level comparator.
* Save as format with transparency support, like add `.png` file name suffix.

Live preview
------------
As `magick display` does not have live update, I provide `sxiv` startup. Please use `a`, `A` and `+` keys to control its pixel-precise behaviour. 

Note that current `sxiv` is of very lean of CLI options, sadly. As its background color is white, and there is not possible to state background color using CLI, be aware of eyes health and switch to transparent background by hand using key above. This is also reason to use small 64x64 initial window; once background color changed to safe one, one may then to enlarge window.

File save
---------
There is no auto save nor check on exit. You should save each time by hand. There is also preview copy at `/dev/shm`.

TODO
----
* Shift canvas. It is not easy to get Shift nor Caps state with console, running under window manager; maybe with `ncurses` can be possible.
* Add keyboard control for not to roll over out-of-bounds pixels (aka toroidal or endless canvas).

Addendum 1: Low color images
============================
There are multiple reasons to know _and have_ these limitations.
They include:
 * Color quantity limit, aka Pixel (data) bit depth limit (or just **bit depth** or **depth**);
 * Color bit depth limit;
 * Or both.

The simplest image use just one bit per pixel. It is enough to have black and white images, where 0 is black and 1 is white (on display; opposite at printing). But if we define other rule, like green for 0 and pink for 1, we can encode other colors with same amount of data. This rule is called **palette**. If we join this palette with our bit-coded raster array, it will be _indexed format_ of image. It is essentially important to understand and to know it. Indexed means the raster array represents not colors itself, but their indexes, or just numbers in pallette; palette then is (standard or own) color list table of size 2^n, where n is bit **depth** of raster array data; n thus defines color quantity. It is not related to _color depth_. In our case here, depth is 1 (only one bit per pixel), so we need 2^1 = 2 colors palette, like green and pink above.

The colors itself are not obligated to be low bit depth; they today most often will be 24-bit (like 0x117722 for some green) or 32-bit (like 0x7f117722 for same yet with 50% _opacity_). But still there are reasons to limit _palette color bit depth_ also; it is important to know how to deal with it.
Note that when image is indexed, and palette used is own (non regular), and, this palette is lost or omitted, then data array may be decoded to full range (pure black to pure white) grayscale: in our case of 1-bit image, it will be black and white image. So, palette _can be_ sorted by colors _perceived brightness_, so if it is lost or can't be used, we still can see something in grayscaled form, as a fallback.

Now let's take some natural full color image, where each pixel is most often 24 bit depth (aka RGB888, TrueColor, or full color). To encode it as indexed, we will need to create its palette first. It can be easily noted that our color list size is quickly outgrows 256 entries (adequate maximum of practically useful palettes). So, _color reduction_ (**not** _bit depth reduction_) is need; and for natural images, it is out of scope here, but in simplest form, it is "just" aggregating of similar pixels and replace all to one median color).

The fundamental reason for convert to indexed format, and thus color quantity reduction, is, these images are can be way more effectively losslessly compressed. Lossless compression is not mandatory for natural images. But, if compression is need, lossless one is unavoidable for icons, interface elements, bitmap fonts (most notably for embedded platforms and A grade measurement units), maps, weather and medical imaging, and more.

The more color quantity reduction, the more effective compression. 1-bit images are best for that, and furthermore, they are fastest: RLE decompression is as fast (or even _faster_) as memcpy(), and most effective for like font glyphs. As palette can be applied while decompression, it is easy to have colored fonts without storage or speed losses. Transparent or opaque background of glyphs is also not eats extra CPU or memory.

Example is `.gif` image format: it is losslessly compressed, indexed, 256 color format, with one color index can be used for 100% transparency.

While editing, most regular color changing operations are impossible for indexed images, unless editor uses some full color internal format for that; it is then important to deeply examine indexed result after save. Again, most fundamental is to be familiar with direct color to indexed, and back, conversion, and to know which exactly occurs with raster array data then.

At this point we know, and this is most useful today at powerful/desktop platforms, that indexed images are can have 1 to 8 bit depth, so 2 to 256 colors. While colors itself are full color and may be transparent. The palette can be own, or some standard pre-defined table, like 216 web-safe colors. To know pixel (data, or index) bit depth of image, is of essentiall importance.

Next level of understanding is when we need to reduce _color bit depth_. Now every color in palette can't be any of 2^24 values, but only from well limited set, and this set most often only useful when it is well settled and/or standartized, which allows not to store palette with images files. Like mentioned websafe-216 set, it also can be 6-bit (EGA64 aka RGB222) or 4-bit (EGA16 aka RGBI), or even 3-trit with 27 colors. All these sets are not includes transparency, and this is reason why there is not too much indexed file formats w/o palettes (intended to be decoded using pre-defined one); like `.pcx`, they are mostly forgotten now.

Other problem is transparent "color" position in color table is not settled yet. It can be first, last, or any; more often it is last one, like for EGA16 it will be 17th color. While its index can be 16 in this case, we will use index 255 for our internal data array (it is byte based) for transparency always, as it will be then same for 1- to 255-color images.

To make fast and effective color selection with keyboard driven image editor, it is need to reduce colors quantity. To make color selection experience consistent between images, it is need to use same palette. The most adequate for this, is 4-bit EGA16 palette; it is also used on most, if not all, color terminals. We then use `0`..`9`, `a`..`f` to select color, and `Del` or `BkSp` to select transparent color. It is also well suitable for 1-bit images. `Tab` used for invert current color.

It is excellent setup to create or edit icons and fonts.

Now we want to process some image.
Note: For 1-bit image, commands will be same; yet finally one need to convert output `.pam` file to 1-bit format of your choice, it will be losslessly.
First, we need to decimate its colors to EGA16 palette.
Possible preprocessing step is to decimate colors with graphics editor.
This command also will convert it to regular .pam format (we use it for load and save):

    magick test.ico -depth 2 test.pam
    magick display test.pam   # Use middle button for no-blur zoom.

The resulting image will be 2 bit _per channel_, aka 6-bit, or EGA64 palette (also for 1-bit source). Btw, note that `.pam` file format itself is not indexed, we use it due to it's simple and no any dependency need.

For color image, next fundamental trouble is 1) how to convert these 6-bit colors to 4-bit indexes, yet 2) have full palette defined, not just first 3 entries if we have black, red, and white pixels. It is not solved with current image processors; or, in other words, the standartized palettes is not supported; mostly due to there is no current file format support for that (see above). So, we do it internally. As there is 64 → 16 colors decimation, it is your task to, before actual editing, just save file, (back conversion will be applied, but colors are lost), check if result is correct, and repeat from step above (decimating colors with graphics processor with changed tresholds).

Addendum 2: EGA16 palette
=========================
This is 4 bit, RGBI, 16 colors total palette, without transparency.

Along of three `RGB` bits, one extra `I` bit means intensity, and it modulate all three RGB channels.
There are at least two obvious ways of modulation, to sum and to multiply.

Multiplying was used on ZX Spectrum video system. It does not allow gray color: black multiplied by any value, remains black. So there was only 15 colors of 16.

IBM uses `I` bit as summing with about 1/2 of full-range CRT cathode voltages (note again: adds to all RGB bits). This gives full 16 colors set. But unlike of ZX Spectrum, this does not allow pure colors (like 100% red) _today_. Half of century ago, it was like (really) bright red, and even more bright (more than 100% in modern terms) when `I` bit was set; CRTs allows for that. To squeeze it into present time hard-limited range of like `0x00` to `0xff` per channel, one uses `0x00` and `0xaa` values for RGB channels when `I` bit cleared, and `0x55` and `0xff` when `I` set; in other words, we add 1/3 of full range. It is can be easily noted that pure color like red 0x0000ff is impossible. Btw, one also may note that colors which differs by `I` bit, are referred like "same chrominance, different luminance" [^1].

To create this palette image:

    { echo -n 'P3 16 1 3 '; echo 000002020022200202220222111113131133311313331333 | sed 's/./& /g'; } | magick pnm:- -filter point -resize 3200% out.png

    magick display out.png

Addendum 3: EGA16 palette with brown color
==========================================
There was one irregular color in 4-bit palette. IBM decided to replace dark yellow to brown color, as it was much better and eye friendly for text modes, when dark part of palette was often used for background. As it was made _inside of display monitor_ using TTL logic, it was impossible to change, unless 4 TTL wires were replaced to 6. Brown color still stays default forever, but it (as well as other 15 colors) can be changed now to one of 64 available, incl. dark yellow. To see it, use

    000002020022200202210222111113131133311313331333

in command above. All current color terminal emulators are does not use brown color; this editor also does not, but can be easily added.

[^1]: https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/drivers/video/fbdev/core/fbcon.c

LICENSE
=======
This manual for `lacie.c`, as well as man page and/or images, if any, are licensed under Creative Commons Attribution 4.0. You are welcome to contribute to the manual in order to improve it so long as your contributions are made available under this same license.

`lacie.c` is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

