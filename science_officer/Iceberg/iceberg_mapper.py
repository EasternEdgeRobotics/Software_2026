import math
import tkinter as tk
from tkinter import ttk, messagebox

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure


# ------------------------------------------------------------
# Platform data
# ------------------------------------------------------------
# NOTE:
# The document you pasted appears to have a likely typo for Hibernia:
# it lists 43.7504, but the map places Hibernia near 46.7504.
# This script uses 46.7504.
PLATFORMS = {
    "Hibernia": {
        "lat": 46.7504,
        "lon": -48.7819,
        "depth_m": 78,
    },
    "Hebron": {
        "lat": 46.544,
        "lon": -48.498,
        "depth_m": 93,
    },
    "Sea Rose": {
        "lat": 46.7895,
        "lon": -48.1417,
        "depth_m": 107,
    },
    "Terra Nova": {
        "lat": 46.4,
        "lon": -48.4,
        "depth_m": 91,
    },
}


# ------------------------------------------------------------
# Coordinate/math helpers
# ------------------------------------------------------------
def dms_to_decimal(degrees, minutes, seconds, direction):
    decimal = abs(degrees) + minutes / 60 + seconds / 3600

    if direction in ["South", "West"]:
        decimal *= -1

    return decimal


def latlon_to_xy_nm(lat, lon, reference_lat):
    """
    Converts latitude/longitude to local x/y coordinates in nautical miles.
    """
    x = lon * 60 * math.cos(math.radians(reference_lat))
    y = lat * 60
    return x, y


def xy_nm_to_latlon(x, y, reference_lat):
    lat = y / 60
    lon = x / (60 * math.cos(math.radians(reference_lat)))
    return lat, lon


def heading_to_unit_vector(heading_degrees):
    """
    Heading is degrees clockwise from true north.

    0 = north
    90 = east
    180 = south
    270 = west
    """
    radians = math.radians(heading_degrees)

    dx = math.sin(radians)
    dy = math.cos(radians)

    return dx, dy


def closest_distance_to_ray_nm(start_x, start_y, dir_x, dir_y, point_x, point_y):
    """
    Finds the closest distance from a platform to the iceberg path.

    The iceberg path is treated as a ray starting at the iceberg's location
    and extending forward along its heading.
    """
    vector_x = point_x - start_x
    vector_y = point_y - start_y

    along_track_nm = vector_x * dir_x + vector_y * dir_y

    if along_track_nm < 0:
        closest_x = start_x
        closest_y = start_y
    else:
        closest_x = start_x + along_track_nm * dir_x
        closest_y = start_y + along_track_nm * dir_y

    distance_nm = math.hypot(point_x - closest_x, point_y - closest_y)

    return distance_nm, along_track_nm, closest_x, closest_y


# ------------------------------------------------------------
# Threat calculations
# ------------------------------------------------------------
def surface_threat_level(distance_nm, keel_depth_m, water_depth_m):
    """
    Surface platform rules:

    If keel depth is 110% or greater than water depth, the iceberg grounds
    before reaching the platform, so the threat is green.

    Otherwise:
    > 10 nm = green
    5 to 10 nm = yellow
    < 5 nm = red
    """
    if keel_depth_m >= 1.1 * water_depth_m:
        return "Green"

    if distance_nm < 5:
        return "Red"

    if distance_nm <= 10:
        return "Yellow"

    return "Green"


def subsea_threat_level(distance_nm, keel_depth_m, water_depth_m):
    """
    Subsea asset rules:

    If the iceberg does not pass within 25 nautical miles, it is green.

    If within 25 nautical miles:
    keel >= 110% of depth = green
    keel 90% to 110% of depth = red
    keel 70% to 90% of depth = yellow
    keel < 70% of depth = green
    """
    if distance_nm > 25:
        return "Green"

    ratio = keel_depth_m / water_depth_m

    if ratio >= 1.1:
        return "Green"

    if ratio >= 0.9:
        return "Red"

    if ratio >= 0.7:
        return "Yellow"

    return "Green"


def threat_color(threat):
    colors = {
        "Green": "#2ca02c",
        "Yellow": "#d9a300",
        "Red": "#d62728",
    }

    return colors.get(threat, "black")

def threat_marker(threat):
    markers = {
        "Green": "🟩",
        "Yellow": "🟨",
        "Red": "🟥",
    }

    return f"{markers.get(threat, '⬛')} {threat}"


def worst_threat(surface_threat, subsea_threat):
    order = {
        "Green": 0,
        "Yellow": 1,
        "Red": 2,
    }

    if order[subsea_threat] > order[surface_threat]:
        return subsea_threat

    return surface_threat


# ------------------------------------------------------------
# Main GUI app
# ------------------------------------------------------------
class IcebergThreatApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Iceberg Threat Level Calculator")

        self.create_variables()
        self.create_layout()

    def create_variables(self):
        self.lat_deg = tk.StringVar(value="")
        self.lat_min = tk.StringVar(value="")
        self.lat_sec = tk.StringVar(value="")
        self.lat_dir = tk.StringVar(value="North")

        self.lon_deg = tk.StringVar(value="")
        self.lon_min = tk.StringVar(value="")
        self.lon_sec = tk.StringVar(value="")
        self.lon_dir = tk.StringVar(value="West")

        self.heading = tk.StringVar(value="")
        self.keel_depth = tk.StringVar(value="")

    def create_layout(self):
        main_frame = ttk.Frame(self.root, padding=10)
        main_frame.pack(fill=tk.BOTH, expand=True)

        input_frame = ttk.LabelFrame(main_frame, text="Iceberg Information")
        input_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 10))

        plot_frame = ttk.Frame(main_frame)
        plot_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        self.create_inputs(input_frame)
        self.create_plot(plot_frame)
        self.create_table(plot_frame)

    def create_inputs(self, parent):
        ttk.Label(parent, text="Latitude").grid(
            row=0,
            column=0,
            columnspan=4,
            sticky="w",
            pady=(5, 2),
        )

        ttk.Entry(parent, textvariable=self.lat_deg, width=6).grid(
            row=1,
            column=0,
            padx=2,
        )
        ttk.Label(parent, text="°").grid(row=1, column=1)

        ttk.Entry(parent, textvariable=self.lat_min, width=6).grid(
            row=1,
            column=2,
            padx=2,
        )
        ttk.Label(parent, text="'").grid(row=1, column=3)

        ttk.Entry(parent, textvariable=self.lat_sec, width=6).grid(
            row=1,
            column=4,
            padx=2,
        )
        ttk.Label(parent, text='"').grid(row=1, column=5)

        ttk.OptionMenu(
            parent,
            self.lat_dir,
            self.lat_dir.get(),
            "North",
            "South",
        ).grid(row=1, column=6, padx=2)

        ttk.Label(parent, text="Longitude").grid(
            row=2,
            column=0,
            columnspan=4,
            sticky="w",
            pady=(15, 2),
        )

        ttk.Entry(parent, textvariable=self.lon_deg, width=6).grid(
            row=3,
            column=0,
            padx=2,
        )
        ttk.Label(parent, text="°").grid(row=3, column=1)

        ttk.Entry(parent, textvariable=self.lon_min, width=6).grid(
            row=3,
            column=2,
            padx=2,
        )
        ttk.Label(parent, text="'").grid(row=3, column=3)

        ttk.Entry(parent, textvariable=self.lon_sec, width=6).grid(
            row=3,
            column=4,
            padx=2,
        )
        ttk.Label(parent, text='"').grid(row=3, column=5)

        ttk.OptionMenu(
            parent,
            self.lon_dir,
            self.lon_dir.get(),
            "West",
            "East",
        ).grid(row=3, column=6, padx=2)

        ttk.Label(parent, text="Heading, degrees").grid(
            row=4,
            column=0,
            columnspan=4,
            sticky="w",
            pady=(15, 2),
        )

        ttk.Entry(parent, textvariable=self.heading, width=12).grid(
            row=5,
            column=0,
            columnspan=3,
            sticky="w",
            padx=2,
        )

        ttk.Label(parent, text="Keel Depth, meters").grid(
            row=6,
            column=0,
            columnspan=4,
            sticky="w",
            pady=(15, 2),
        )

        ttk.Entry(parent, textvariable=self.keel_depth, width=12).grid(
            row=7,
            column=0,
            columnspan=3,
            sticky="w",
            padx=2,
        )

        update_button = ttk.Button(
            parent,
            text="Update Plot",
            command=self.update_plot,
        )
        update_button.grid(
            row=8,
            column=0,
            columnspan=7,
            sticky="ew",
            pady=(20, 5),
        )

        sample_button = ttk.Button(
            parent,
            text="Load Sample Values",
            command=self.load_sample_values,
        )
        sample_button.grid(
            row=9,
            column=0,
            columnspan=7,
            sticky="ew",
            pady=5,
        )

        help_text = (
            "Example input:\n"
            "47°39'00\" North\n"
            "48°37'00\" West\n"
            "Heading: 158°\n"
            "Keel Depth: 99 m"
        )

        ttk.Label(
            parent,
            text=help_text,
            justify=tk.LEFT,
        ).grid(
            row=10,
            column=0,
            columnspan=7,
            sticky="w",
            pady=(20, 5),
        )

    def create_plot(self, parent):
        self.figure = Figure(figsize=(8, 6), dpi=100)
        self.ax = self.figure.add_subplot(111)

        self.canvas = FigureCanvasTkAgg(self.figure, master=parent)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def create_table(self, parent):
        table_frame = ttk.LabelFrame(parent, text="Threat Level Results")
        table_frame.pack(fill=tk.X, pady=(10, 0))

        columns = (
            "platform",
            "closest_pass",
            "water_depth",
            "surface_threat",
            "subsea_threat",
        )

        self.table = ttk.Treeview(
            table_frame,
            columns=columns,
            show="headings",
            height=5,
        )

        self.table.heading("platform", text="Platform")
        self.table.heading("closest_pass", text="Closest Pass")
        self.table.heading("water_depth", text="Water Depth")
        self.table.heading("surface_threat", text="Surface Threat")
        self.table.heading("subsea_threat", text="Subsea Threat")

        self.table.column("platform", width=120, anchor=tk.CENTER)
        self.table.column("closest_pass", width=120, anchor=tk.CENTER)
        self.table.column("water_depth", width=120, anchor=tk.CENTER)
        self.table.column("surface_threat", width=130, anchor=tk.CENTER)
        self.table.column("subsea_threat", width=130, anchor=tk.CENTER)

        self.table.pack(fill=tk.X)

    def load_sample_values(self):
        self.lat_deg.set("47")
        self.lat_min.set("39")
        self.lat_sec.set("00")
        self.lat_dir.set("North")

        self.lon_deg.set("48")
        self.lon_min.set("37")
        self.lon_sec.set("00")
        self.lon_dir.set("West")

        self.heading.set("158")
        self.keel_depth.set("99")

        self.update_plot()

    def read_inputs(self):
        try:
            lat = dms_to_decimal(
                float(self.lat_deg.get()),
                float(self.lat_min.get()),
                float(self.lat_sec.get()),
                self.lat_dir.get(),
            )

            lon = dms_to_decimal(
                float(self.lon_deg.get()),
                float(self.lon_min.get()),
                float(self.lon_sec.get()),
                self.lon_dir.get(),
            )

            heading = float(self.heading.get())
            keel_depth_m = float(self.keel_depth.get())

            if not 0 <= heading < 360:
                raise ValueError("Heading must be between 0 and 359.999 degrees.")

            if keel_depth_m < 0:
                raise ValueError("Keel depth must be positive.")

            return lat, lon, heading, keel_depth_m

        except ValueError as error:
            messagebox.showerror("Invalid Input", str(error))
            return None

    def calculate_results(self, iceberg_lat, iceberg_lon, heading, keel_depth_m):
        reference_lat = iceberg_lat

        iceberg_x, iceberg_y = latlon_to_xy_nm(
            iceberg_lat,
            iceberg_lon,
            reference_lat,
        )

        dir_x, dir_y = heading_to_unit_vector(heading)

        results = []

        for name, data in PLATFORMS.items():
            platform_x, platform_y = latlon_to_xy_nm(
                data["lat"],
                data["lon"],
                reference_lat,
            )

            distance_nm, along_track_nm, closest_x, closest_y = (
                closest_distance_to_ray_nm(
                    iceberg_x,
                    iceberg_y,
                    dir_x,
                    dir_y,
                    platform_x,
                    platform_y,
                )
            )

            surface_threat = surface_threat_level(
                distance_nm,
                keel_depth_m,
                data["depth_m"],
            )

            subsea_threat = subsea_threat_level(
                distance_nm,
                keel_depth_m,
                data["depth_m"],
            )

            results.append(
                {
                    "name": name,
                    "lat": data["lat"],
                    "lon": data["lon"],
                    "depth_m": data["depth_m"],
                    "distance_nm": distance_nm,
                    "along_track_nm": along_track_nm,
                    "closest_x": closest_x,
                    "closest_y": closest_y,
                    "surface_threat": surface_threat,
                    "subsea_threat": subsea_threat,
                }
            )

        return results

    def update_table(self, results):
        for item in self.table.get_children():
            self.table.delete(item)

        for result in results:
            self.table.insert(
                "",
                tk.END,
                values=(
                    result["name"],
                    f"{result['distance_nm']:.1f} nm",
                    f"{result['depth_m']} m",
                    threat_marker(result["surface_threat"]),
                    threat_marker(result["subsea_threat"]),
                ),
            )

    def update_plot(self):
        inputs = self.read_inputs()

        if inputs is None:
            return

        iceberg_lat, iceberg_lon, heading, keel_depth_m = inputs
        reference_lat = iceberg_lat

        results = self.calculate_results(
            iceberg_lat,
            iceberg_lon,
            heading,
            keel_depth_m,
        )

        self.update_table(results)

        self.ax.clear()

        iceberg_x, iceberg_y = latlon_to_xy_nm(
            iceberg_lat,
            iceberg_lon,
            reference_lat,
        )

        dir_x, dir_y = heading_to_unit_vector(heading)

        path_length_nm = 150
        end_x = iceberg_x + dir_x * path_length_nm
        end_y = iceberg_y + dir_y * path_length_nm
        end_lat, end_lon = xy_nm_to_latlon(end_x, end_y, reference_lat)

        self.ax.plot(
            [iceberg_lon, end_lon],
            [iceberg_lat, end_lat],
            color="blue",
            linewidth=2.5,
            label=f"Iceberg Path, {heading:.0f}°",
        )

        self.ax.scatter(
            iceberg_lon,
            iceberg_lat,
            s=130,
            color="cyan",
            edgecolor="black",
            zorder=5,
            label="Iceberg Start",
        )

        self.ax.annotate(
            "Iceberg Start",
            (iceberg_lon, iceberg_lat),
            xytext=(8, 8),
            textcoords="offset points",
            fontsize=9,
            weight="bold",
        )

        for result in results:
            platform_color = threat_color(result["surface_threat"])

            self.ax.scatter(
                result["lon"],
                result["lat"],
                s=120,
                color=platform_color,
                edgecolor="black",
                zorder=5,
            )

            self.ax.annotate(
                result["name"],
                (result["lon"], result["lat"]),
                xytext=(8, 8),
                textcoords="offset points",
                fontsize=9,
                weight="bold",
            )

            closest_lat, closest_lon = xy_nm_to_latlon(
                result["closest_x"],
                result["closest_y"],
                reference_lat,
            )

            self.ax.plot(
                [result["lon"], closest_lon],
                [result["lat"], closest_lat],
                color="gray",
                linestyle="--",
                linewidth=1,
                alpha=0.7,
            )

        all_lats = [iceberg_lat, end_lat]
        all_lons = [iceberg_lon, end_lon]

        for platform in PLATFORMS.values():
            all_lats.append(platform["lat"])
            all_lons.append(platform["lon"])

        lat_margin = 0.2
        lon_margin = 0.2

        self.ax.set_xlim(min(all_lons) - lon_margin, max(all_lons) + lon_margin)
        self.ax.set_ylim(min(all_lats) - lat_margin, max(all_lats) + lat_margin)

        self.ax.set_title(
            (
                "Iceberg Track and Platform Threat Levels\n"
                f"Start: {iceberg_lat:.4f}, {iceberg_lon:.4f} | "
                f"Heading: {heading:.0f}° | "
                f"Keel Depth: {keel_depth_m:.0f} m"
            ),
            fontsize=12,
            weight="bold",
        )

        self.ax.set_xlabel("Longitude")
        self.ax.set_ylabel("Latitude")
        self.ax.grid(True, linestyle="--", alpha=0.5)
        self.ax.legend(loc="upper right")

        self.ax.set_aspect("auto")
        self.figure.tight_layout()

        self.canvas.draw()


if __name__ == "__main__":
    root = tk.Tk()
    app = IcebergThreatApp(root)
    root.mainloop()