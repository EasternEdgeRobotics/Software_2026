import cv2
import numpy as np 
import os
import sys
import time
import copy
import datetime


def extract_table_to_csv(image, table, debug = False, feedback=""):
    """
    Attempts to extract invasive carp migration data from an image.
    """

    gray_image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

    # All pixel-based meaurements were based on an input image with resolution 640x480
    # Thus, a scaling factor is needed incase the input image is of a different resolution
    vertical_scaling_factor, horizontal_scaling_factor = int(gray_image.shape[0]/480), int(gray_image.shape[1]/640)

    if debug:
        cv2.imwrite(f"gray.jpg", gray_image)

    # Turn the image into black and white (values obtained through trial and error)
    thresh = cv2.adaptiveThreshold(gray_image, 255, cv2.ADAPTIVE_THRESH_MEAN_C,
                                   cv2.THRESH_BINARY_INV, 17, 4)
    if debug:
        cv2.imwrite(f"thresholded.jpg", thresh)

    # Create elements to use for detecting vertical and horizontal lines
    vertical_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 30*vertical_scaling_factor))
    horizontal_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (50*horizontal_scaling_factor, 1))

    # Detect horizontal lines
    horizontal_lines = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, horizontal_kernel, iterations=1)

    # Find all horizontal line contours
    horizontal_line_contours, _ = cv2.findContours(horizontal_lines, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    
    # Get the smallest bounding rectangle of each contour
    horizontal_line_rectangles = [cv2.boundingRect(c) for c in horizontal_line_contours]

    if debug:
        # Sort the horizontal_line_rectangles lines bottom-to-top, then right-to-left
        horizontal_line_rectangles = sorted(horizontal_line_rectangles, key=lambda b: (b[1], b[0]), reverse=True) 
        all_horizontal_lines_image = image.copy()
        for line_number, (x, y, w, h) in enumerate(horizontal_line_rectangles):
            cv2.rectangle(all_horizontal_lines_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(all_horizontal_lines_image, str(line_number), (x + w + 10, y + h // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)
        cv2.imwrite(f"all_horizontal_lines.jpg", all_horizontal_lines_image) 

    len_before = len(horizontal_line_rectangles)
    
    # Remove horizontal lines with large heights and widths less than 60% of the screen
    # Large heights are filtered out since they indicate a tilted image
    horizontal_line_rectangles = [
        (x, y, w, h) for (x, y, w, h) in horizontal_line_rectangles
        if not (h > 5 * vertical_scaling_factor or w < 0.6 * image.shape[1])
    ]

    print(len(horizontal_line_rectangles),len_before)
    # Sort horizontal lines bottom to up
    horizontal_line_rectangles_sorted = sorted(horizontal_line_rectangles, key=lambda b: b[1], reverse=True)

    # Only consider the bottom 15 horizontal lines to potentially avoid noise at the top of the image
    # This might not be useful, but no problem in keeping it if usability is not impacted 
    if len(horizontal_line_rectangles_sorted) > 15:
        horizontal_line_rectangles_sorted = horizontal_line_rectangles_sorted[:15] 


    ######################################################
    # Attempt to find 11 bottommost same-space wide horizontal lines
    ######################################################

    # Sort bottom to up
    horizontal_line_rectangles_sorted = sorted(horizontal_line_rectangles_sorted, key=lambda b: b[1], reverse=True)

    table_row_line_rectangles = []
    largest_suitable_group_length = 0
    width_ratio_min = 0.8
    width_ratio_max = 1.6

    # Starting on the bottom line and moving up
    for start in range(len(horizontal_line_rectangles_sorted)):
        current_group_of_same_space_lines = [horizontal_line_rectangles_sorted[start]]
        
        # Check all lines that are above this one
        for next_idx in range(start + 1, len(horizontal_line_rectangles_sorted)):

            # Get y position and width of the lower line
            lower_line_y = current_group_of_same_space_lines[-1][1]
            lower_line_width = current_group_of_same_space_lines[-1][2]

            # Get y and width of higher line
            higher_line_y = horizontal_line_rectangles_sorted[next_idx][1]
            higher_line_width = horizontal_line_rectangles_sorted[next_idx][2]
            
            width_ratio = higher_line_width/lower_line_width

            if len(current_group_of_same_space_lines) == 1:
                # If we are on the second line, we need to figure out the spacing 
                spacing = abs(lower_line_y - higher_line_y)
            else:
                # If we are beyond the second line, use the spacing of the first and second line as the spacing to match
                spacing = abs(current_group_of_same_space_lines[-1][1] - current_group_of_same_space_lines[-2][1])

            expected_y_position_of_higher_line = current_group_of_same_space_lines[-1][1] - spacing

            if abs(higher_line_y - expected_y_position_of_higher_line) <= 10*horizontal_scaling_factor and width_ratio >= width_ratio_min and width_ratio <= width_ratio_max:
                current_group_of_same_space_lines.append(horizontal_line_rectangles_sorted[next_idx])
            if len(current_group_of_same_space_lines) == 11:
                break
        if len(current_group_of_same_space_lines) == 11:
            table_row_line_rectangles = current_group_of_same_space_lines
            break
        if len(current_group_of_same_space_lines) > largest_suitable_group_length:
            largest_suitable_group_length = len(current_group_of_same_space_lines)
            table_row_line_rectangles = current_group_of_same_space_lines


    if not len(table_row_line_rectangles) == 11:
        # Draw all the table cell row lines we obtained thus far
        feedback_image = image.copy()
        for line_number, (x, y, w, h) in enumerate(table_row_line_rectangles):
            cv2.rectangle(feedback_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(feedback_image, str(line_number), (x + w + 10, y + h // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)
        if not (feedback == "Not enough horizontal lines"):
            print("Not enough horizontal lines")
        return "Not enough horizontal lines", False, table, feedback_image

    # Draw contours on the original image for visualization
    if debug:
        table_row_lines_image = image.copy()
        for line_number, (x, y, w, h) in enumerate(table_row_line_rectangles):
            cv2.rectangle(table_row_lines_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(table_row_lines_image, str(line_number), (x + w + 10, y + h // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)
        cv2.imwrite(f"table_row_lines.jpg", table_row_lines_image)

    ######################################################
    ######################################################
    ######################################################

    # Detect vertical lines 
    vertical_lines = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, vertical_kernel, iterations=1)
    if debug:
        cv2.imwrite(f"all_vertical_lines.jpg", vertical_lines)

    # Find all vertical line contours
    vertical_line_contours, _ = cv2.findContours(vertical_lines, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    
    # Get the smallest bounding rectangle of each contour
    vertical_line_rectangles = [cv2.boundingRect(c) for c in vertical_line_contours]

    # Sort the vertical_line_rectangles lines left-to-right, then top-to-bottom
    vertical_line_rectangles = sorted(vertical_line_rectangles, key=lambda b: (b[0], b[1]))  

    

    ######################################################
    # Combine overlapping vertical lines
    ######################################################

    # It seems that vertical lines are often split up
    # Combine overlapping/close vertical lines that are at the same x value
    combined_vertical_lines = []
    current_line_index = 0
    while current_line_index < len(vertical_line_rectangles):
        x, y, w, h = vertical_line_rectangles[current_line_index]
        # Check if next line is close enough to merge
        while current_line_index + 1 < len(vertical_line_rectangles):
            nx, ny, nw, nh = vertical_line_rectangles[current_line_index + 1]
            # If the next line is close in x (horizontal) and overlaps or is close in y (vertical), merge
            if ((abs(nx - (x + w)) < 10*horizontal_scaling_factor or abs(nx - x) < 10*horizontal_scaling_factor) and 
                (abs(y+h-ny) < 45*vertical_scaling_factor or abs(ny+nh-y)<45*vertical_scaling_factor)):
                # Merge rectangles
                mx = min(x, nx)
                my = min(y, ny)
                mw = max(x + w, nx + nw) - mx
                mh = max(y + h, ny + nh) - my
                x, y, w, h = mx, my, mw, mh
                # Move to next line right away to see if we can merge yet another line with the current one
                current_line_index += 1  
            else:
                break
        combined_vertical_lines.append((x, y, w, h))

        # Increasing the current line index here will get us to the next unmerged line
        current_line_index += 1
    vertical_line_rectangles = combined_vertical_lines

    # Remove vertical lines that are too short (i.e. noise)
    vertical_line_rectangles = [rect for rect in vertical_line_rectangles if rect[2] <= 10*vertical_scaling_factor]

    if len(vertical_line_rectangles) > 12:

        # Re-sort the vertical lines from largest to smallest
        vertical_line_rectangles_sorted = sorted(vertical_line_rectangles, key=lambda b: b[3], reverse=True)

        # Take only the top twelve vertical lines to avoid noise
        vertical_line_rectangles = vertical_line_rectangles_sorted[:12] if len(vertical_line_rectangles_sorted) > 12 else vertical_line_rectangles_sorted.copy()
    
    ######################################################
    # Attempt to find the 6 rightmost same-space high vertical lines
    ####################################################### 
    
    # Sort right-to-left (by x, descending)
    vertical_line_rectangles_sorted = sorted(vertical_line_rectangles, key=lambda b: b[0], reverse=True)

    # Try to find 6 lines that are equally spaced (within a tolerance)
    table_column_line_rectangles = []
    largest_suitable_group_length = 0
    height_ratio_min = 0.8
    height_ratio_max = 1.6
    
    # Starting from the right line and moving left
    for start in range(len(vertical_line_rectangles_sorted)):
        current_group_of_same_space_lines = [vertical_line_rectangles_sorted[start]]

        # Check all lines that are to the left of this one
        for next_idx in range(start + 1, len(vertical_line_rectangles_sorted)):

            # Get x position and height of the right line
            last_x = current_group_of_same_space_lines[-1][0]
            last_height = current_group_of_same_space_lines[-1][3]

            # Get x position and height of the left line
            next_x = vertical_line_rectangles_sorted[next_idx][0]
            next_height = vertical_line_rectangles_sorted[next_idx][3]

            height_ratio = next_height/last_height

            if len(current_group_of_same_space_lines) == 1:
                # If we are on the second line, we need to figure out the spacing
                spacing = abs(last_x - next_x)
            else:
                # If we are beyond the second line, use the spacing of the first and second line as the spacing to match
                spacing = abs(current_group_of_same_space_lines[-1][0] - current_group_of_same_space_lines[-2][0])
            
            expected_x = current_group_of_same_space_lines[-1][0] - spacing
            
            if abs(next_x - expected_x) <= 10*horizontal_scaling_factor and height_ratio >= height_ratio_min and height_ratio <= height_ratio_max:
                current_group_of_same_space_lines.append(vertical_line_rectangles_sorted[next_idx])
            if len(current_group_of_same_space_lines) == 6:
                break
        if len(current_group_of_same_space_lines) == 6:
            table_column_line_rectangles = current_group_of_same_space_lines
            break
        if len(current_group_of_same_space_lines) > largest_suitable_group_length:
            largest_suitable_group_length = len(current_group_of_same_space_lines)
            table_column_line_rectangles = current_group_of_same_space_lines

    feedback_image = image.copy()
    for line_number, (x, y, w, h) in enumerate(table_column_line_rectangles + table_row_line_rectangles):
        cv2.rectangle(feedback_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
        cv2.putText(feedback_image, str(line_number), (x + w + 10, y + h // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)
    
    # Check if there is a vertical line at the rightmost point of a horizontal line
    # This indicates that this is indeed the table we are looking for in terms of dimensions
    right_table_boundry_consistent = False

    if table_row_line_rectangles and table_column_line_rectangles:
        rightmost_hline = max(table_row_line_rectangles, key=lambda b: b[0] + b[2])
        rightmost_hx_start = rightmost_hline[0]
        rightmost_hx_end = rightmost_hline[0] + rightmost_hline[2]
        rightmost_vline = max(table_column_line_rectangles, key=lambda b: b[0] + b[2])
        rightmost_vx_start = rightmost_vline[0] 
        rightmost_vx_end = rightmost_vline[0] + rightmost_vline[2]
        if (rightmost_vx_start >= rightmost_hx_start and rightmost_vx_start <= rightmost_hx_end) or (rightmost_vx_end >= rightmost_hx_start and rightmost_vx_end <= rightmost_hx_end):
            right_table_boundry_consistent = True

    if not right_table_boundry_consistent:
        if not (feedback == "Trying to establish right table boundry"):
            print("Trying to establish right table boundry")
        return "Trying to establish right table boundry", False, table, feedback_image

    feedback_image = image.copy()
    for line_number, (x, y, w, h) in enumerate(table_column_line_rectangles + table_row_line_rectangles):
        cv2.rectangle(feedback_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
        cv2.putText(feedback_image, str(line_number), (x + w + 10, y + h // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)

    if not len(table_column_line_rectangles) == 6:
        if not (feedback == "Not enough vertical lines"):
            print("Not enough vertical lines")
        return "Not enough vertical lines", False, table, feedback_image

    # Draw total table
    feedback_image = image.copy()
    for (x, y, w, h) in table_column_line_rectangles + table_row_line_rectangles:
        cv2.rectangle(feedback_image, (x, y), (x + w, y + h), (0, 255, 0), 2)
    
    # Draw contours on the original image for visualization
    if debug:
        cv2.imwrite(f"table_extracted.jpg", feedback_image)

    # Ensure the output directory exists

    os.makedirs(f"table_cells", exist_ok=True)

    num_table_row_lines = len(table_row_line_rectangles)
    num_table_column_lines = len(table_column_line_rectangles)

    for row_number in range(num_table_row_lines-1):
        
        # We have to address in reverse since lines are organized bottom-up
        row_start = table_row_line_rectangles[num_table_row_lines-row_number-1][1] 
        row_end = table_row_line_rectangles[num_table_row_lines-row_number-2][1]

        for column_number in range(num_table_column_lines-1):
            
            # We have to address in reverse since lines are organized left-right
            column_start = table_column_line_rectangles[num_table_column_lines-column_number-1][0] + table_column_line_rectangles[num_table_column_lines-column_number-1][2]
            column_end = table_column_line_rectangles[num_table_column_lines-column_number-2][0]

            # Crop the cell from the original image using the calculated start and end positions
            cell = image[row_start:row_end, column_start:column_end]

            h, w, _ = cell.shape

            cell_gray = cv2.cvtColor(cell, cv2.COLOR_BGR2GRAY)

            # Adaptive thresholding parameters obtained through experimentation
            cell_thresh = cv2.adaptiveThreshold(
                cell_gray, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                cv2.THRESH_BINARY, 9, 2
            )

            # Replace the outer pixels with white in case there is a table border
            h, w = cell_thresh.shape
            border_h = max(1, int(h * 0.1))
            border_w = max(1, int(w * 0.1))
            cell_thresh[:border_h, :] = 255
            cell_thresh[-border_h:, :] = 255
            cell_thresh[:, :border_w] = 255
            cell_thresh[:, -border_w:] = 255

            # Remove any continuous black blob of large width
            contours, _ = cv2.findContours(255 - cell_thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            cell_thresh_cleaned = cell_thresh.copy()
            for contour in contours:
                x, y, w, h = cv2.boundingRect(contour)
                if w > 0.3 * cell_thresh.shape[1]:
                    cv2.drawContours(cell_thresh_cleaned, [contour], -1, 255, thickness=cv2.FILLED)
            cell_thresh = cell_thresh_cleaned
            
            def get_clean_cell(num_blobs = 1):
                # Remove noise: keep only the largest connected components (biggest black blobs)

                # Inverse white and black
                cell_inv = cv2.bitwise_not(cell_thresh)

                num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(cell_inv, connectivity=4)
                clean_cell = cell_thresh.copy()
                if num_labels > 1:
                    # Ignore background (label 0), find the three largest components (cv2.CC_STAT_AREA is the index of the array sorted by area)
                    areas = stats[1:, cv2.CC_STAT_AREA]

                    # Get the indices of these largest blobs
                    largest_labels = 1 + np.argsort(areas)[-num_blobs:]  

                    mask = np.zeros_like(cell_inv)
                    for lbl in largest_labels:
                        mask[labels == lbl] = 255

                    # Invert back to original
                    clean_cell = cv2.bitwise_not(mask)
                else:
                    clean_cell = cell_thresh.copy()
                return clean_cell

            non_affirmative_score = None
            affirmative_score = None

            clean_cell = get_clean_cell(2)

            non_affirmative_scores = []
            for template_name in os.listdir("non_affirmative_templates"):
                template_path = os.path.join("non_affirmative_templates", template_name)
                template_img = cv2.imread(template_path, cv2.IMREAD_GRAYSCALE)
                th, tw = template_img.shape
                ch, cw = clean_cell.shape

                # Ensure the extracted image and template image have the same dimensions
                pad_top = max(0, (ch - th) // 2)
                pad_bottom = max(0, ch - th - pad_top)
                pad_left = max(0, (cw - tw) // 2)
                pad_right = max(0, cw - tw - pad_left)
                template_img_padded = cv2.copyMakeBorder(
                    template_img, pad_top, pad_bottom, pad_left, pad_right,
                    borderType=cv2.BORDER_CONSTANT, value=255
                )

                # Compare the template with the obtained image
                res = cv2.matchTemplate(clean_cell, template_img_padded, cv2.TM_CCOEFF_NORMED)
                _, max_val, _, _ = cv2.minMaxLoc(res)
                non_affirmative_scores.append(max_val)
            non_affirmative_score = np.mean(non_affirmative_scores) if non_affirmative_scores else None

            affirmative_scores = []
            for template_name in os.listdir("affirmative_templates"):
                template_path = os.path.join("affirmative_templates", template_name)
                template_img = cv2.imread(template_path, cv2.IMREAD_GRAYSCALE)
                th, tw = template_img.shape
                ch, cw = clean_cell.shape

                # Ensure the extracted image and template image have the same dimensions
                pad_top = max(0, (ch - th) // 2)
                pad_bottom = max(0, ch - th - pad_top)
                pad_left = max(0, (cw - tw) // 2)
                pad_right = max(0, cw - tw - pad_left)
                template_img_padded = cv2.copyMakeBorder(
                    template_img, pad_top, pad_bottom, pad_left, pad_right,
                    borderType=cv2.BORDER_CONSTANT, value=255
                )

                # Compare the template with the obtained image
                res = cv2.matchTemplate(clean_cell, template_img_padded, cv2.TM_CCOEFF_NORMED)
                _, max_val, _, _ = cv2.minMaxLoc(res)
                affirmative_scores.append(max_val)
            affirmative_score = np.mean(affirmative_scores) if affirmative_scores else None
            
            if debug:
                print(max(affirmative_score, non_affirmative_score))

            if affirmative_score > non_affirmative_score:
                text = "Y"
            else:
                text = "N"

            table[row_number+1][column_number+1] = text
            if debug:
                # Save the cropped cell image
                cv2.imwrite(f"table_cells/{row_number+1}_{column_number+1}.png", clean_cell)

    return feedback, True, table, feedback_image


if __name__ == "__main__":
    debug = False
    if "debug" in sys.argv:
        debug = True

    # Split up table into cells based on the extracted boxes.

    table = [
        ["","Region 1", "Region 2", "Region 3", "Region 4", "Region 5"],
        ["2016","?", "?", "?", "?", "?"],
        ["2017","?", "?", "?", "?", "?"],
        ["2018","?", "?", "?", "?", "?"],
        ["2019","?", "?", "?", "?", "?"],
        ["2020","?", "?", "?", "?", "?"],
        ["2021","?", "?", "?", "?", "?"],
        ["2022","?", "?", "?", "?", "?"],
        ["2023","?", "?", "?", "?", "?"],
        ["2024","?", "?", "?", "?", "?"],
        ["2025","?", "?", "?", "?", "?"]
    ]

    def display_table(table):
        print("Current Table:")
        col_headers = ["_____|"] + [f"{i+1:^5}" for i in range(5)]
        print("    " + " ".join(col_headers))
        for i, row in enumerate(table[1:]):
            print(f"({i+2016:2}) {i+1:2}: " + " ".join(f"{cell:^5}" for cell in row[1:]))

    def obtain_table(table):

        cap = cv2.VideoCapture(0)
        frame = None
        if not cap.isOpened():
            print("Cannot open webcam")
            return
        feedback = ""

        while True:
            ret, frame = cap.read()
            last_table = copy.deepcopy(table)
            feedback, done, table, debug_frame = extract_table_to_csv(frame, table, debug, feedback)
            cv2.imshow("Capture", debug_frame)
            if done:
                black_image = np.zeros_like(debug_frame)
                start_time = time.time()
                show_feedback = True
                last_switch_time = time.time()
                while time.time() - start_time < 3:
                    if show_feedback:
                        cv2.imshow("Capture", debug_frame)
                    else:
                        cv2.imshow("Capture", black_image)
                    if time.time() - last_switch_time >= 0.5:
                        last_switch_time = time.time()
                        show_feedback = not show_feedback
                    if cv2.waitKey(int(500)) & 0xFF == ord('q'):
                        break
                break

            if not ret or (cv2.waitKey(1) & 0xFF == ord('q')):
                print("Failed to grab frame")
                table = [
                    ["","Region 1", "Region 2", "Region 3", "Region 4", "Region 5"],
                    ["2016","N", "N", "N", "N", "N"],
                    ["2017","N", "N", "N", "N", "N"],
                    ["2018","N", "N", "N", "N", "N"],
                    ["2019","N", "N", "N", "N", "N"],
                    ["2020","N", "N", "N", "N", "N"],
                    ["2021","N", "N", "N", "N", "N"],
                    ["2022","N", "N", "N", "N", "N"],
                    ["2023","N", "N", "N", "N", "N"],
                    ["2024","N", "N", "N", "N", "N"],
                    ["2025","N", "N", "N", "N", "N"]
                ]
                break
        cap.release()
        cv2.destroyAllWindows()
        return table

    table = obtain_table(table)

    def edit_table(table):
        while True:
            display_table(table)
            print("\nOptions:")
            print("  f<row><col> - flip value at row,col")
            print("  s<row><y/n><y/n><y/n><y/n><y/n>  - Set an entire row")
            print("  c:                 - Continue")
            print("  r:                 - Retry")
            cmd = input("Enter command: ").strip().lower()
            if cmd == "c":
                break
            if cmd.startswith("f"):
                try:
                    _, row, col = cmd.split()
                    row = int(row)
                    col = int(col)
                    if 1 <= row < len(table) and 1 <= col < len(table[0]):
                        current = table[row][col]
                        if current == "Y":
                            table[row][col] = "N"
                        elif current == "N":
                            table[row][col] = "Y"
                        else:
                            print("Cell is not editable (not Y/N).")
                    else:
                        print("Invalid row/col or value.")
                except Exception as e:
                    print("Invalid command format.")
            elif cmd.startswith("s "):
                try:
                    parts = cmd.split()
                    if len(parts) == 3:
                        row = int(parts[1])
                        vals = parts[2].upper()
                        if len(vals) == 5 and all(v.lower() in ("y", "n") for v in vals) and 1 <= row < len(table):
                            for i, v in enumerate(vals):
                                table[row][i+1] = v
                        else:
                            print("Invalid row or values.")
                    else:
                        print("Invalid command format.")
                except Exception as e:
                    print("Invalid command format.")
            elif cmd == "r":
                table = [
                    ["","Region 1", "Region 2", "Region 3", "Region 4", "Region 5"],
                    ["2016","?", "?", "?", "?", "?"],
                    ["2017","?", "?", "?", "?", "?"],
                    ["2018","?", "?", "?", "?", "?"],
                    ["2019","?", "?", "?", "?", "?"],
                    ["2020","?", "?", "?", "?", "?"],
                    ["2021","?", "?", "?", "?", "?"],
                    ["2022","?", "?", "?", "?", "?"],
                    ["2023","?", "?", "?", "?", "?"],
                    ["2024","?", "?", "?", "?", "?"],
                    ["2025","?", "?", "?", "?", "?"]
                ]
                table = obtain_table(table)
            else:
                print("Unknown command.")

    edit_table(table)

    # Create the video
    regions = []
    for i in range(1, 6):
        if os.path.isfile(f"regions/Region{i}.png"):
            region_img = cv2.imread(f"regions/Region{i}.png")
            # Create a mask where red pixels stay red, others become white
            b, g, r = cv2.split(region_img)
            red_mask = (r > 200) & (g < 50) & (b < 50)
            mask = np.ones_like(region_img) * 255  # start with white
            mask[red_mask] = [0, 0, 255]  # set red pixels
            regions.append(mask)


    years = [str(year) for year in range(2016, 2026)]

    for row_number, _ in enumerate(table):
        
        if row_number > 0:
            blank_map = cv2.imread("blank_map.png")
            cv2.putText(
                blank_map,
                years[row_number-1],
                (20, 50),  # x, y position (top left quadrant)
                cv2.FONT_HERSHEY_SIMPLEX,
                1.5,       # font scale
                (0, 0, 0), # black color
                3,         # thickness
                cv2.LINE_AA
            )

            # Create six columns
            for column_number in range(0, 5):
                if table[row_number][column_number+1] == "Y":
                    red_pixels = (regions[column_number][:, :, 2] == 255) & (regions[column_number][:, :, 1] == 0) & (regions[column_number][:, :, 0] == 0)
                    blank_map[red_pixels] = regions[column_number][red_pixels]
            
            # Save the modified map image
            os.makedirs("video_frames", exist_ok=True)
            output_path = os.path.join("video_frames", f"{row_number-1}.png")
            cv2.imwrite(output_path, blank_map)

    image_files = [f"video_frames/{i}.png" for i in range(10)]
    # Read the first image to get frame size
    frame = cv2.imread(image_files[0])
    height, width, layers = frame.shape

    # Define video writer (MP4, 1 fps)
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video = cv2.VideoWriter('output.mp4', fourcc, 1, (width, height))

    for img_file in image_files:
        img = cv2.imread(img_file)
        video.write(img)  # Each image is 1 frame (1 second at 1 fps)
        # Repeat the last frame to make it stay longer
        if img_file == image_files[-1]:
            for _ in range(2):  # Show last frame 2 extra seconds (adjust as needed)
                video.write(img)

    video.release()