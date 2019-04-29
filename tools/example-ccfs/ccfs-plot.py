#!/usr/bin/env python
import pygal
import sys
import csv
from pygal.style import CleanStyle
from pygal.style import Style

style_ccfs = Style(background='transparent', 
    plot_background='transparent', 
    foreground='#000000',
    foreground_strong='#000000',
    foreground_subtle='#006400d',
    opacity='.6',
    opacity_hover='.9',
    transition='100ms ease-in',
    colors=('#006400', '#0022AA', '#FF1111', '#FFEBCD', '#DC143C', 
            '#FFFFFF', 
            '#6495ED', '#B0C4DE', '#D2691E', '#8B0000'))

def write_svg_from_csv(title, outfile, infile, x_head, data_heads, kbps):

    yrange = kbps * 2 


    if kbps >= 2000:
        yrange = 2400
    elif kbps >= 1000:
        yrange = kbps * 1.5


    xy_chart = pygal.XY(file=True, style=style_ccfs, x_title=title, show_dots=False, range=(0,yrange))

    for y_head in data_heads:
        chart_data =[]
        with open(infile) as csv_file:
            csv_reader = csv.DictReader(csv_file)
            for row in csv_reader:
                chart_data += [ (float(row[x_head]), float(row[y_head])) ]

        xy_chart.add(y_head, chart_data)

    svg = outfile + '.svg'
    xy_chart.render_to_file(svg .decode('utf-8'))

    print("  --> Output svg:{}".format(svg))



if __name__=='__main__':
    x_head = "time(s)"
    y_array = ["qdelay", "txed(kbps)", "topo(kbps)"]
    print(" Draw svg")
    print("  - sys.argv[1]: graph title={}" .format(sys.argv[1]))
    print("  - sys.argv[2]: out file name without svg ext = {}".format(sys.argv[2]))
    print("  - sys.argv[3]: input csv file = {}".format(sys.argv[3]))
    print("  - sys.argv[4]: topo(kbps) = {}".format(sys.argv[4]))
    print("  - x head = {} / y array = {}" .format(x_head, y_array))

    write_svg_from_csv(sys.argv[1], sys.argv[2], sys.argv[3], x_head, y_array, int(sys.argv[4]))
