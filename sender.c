/*
    -------------------------
    # A sender program to   #
    # send a video file     #
    # over TCP sockets      #
    -------------------------
    # written in C          #
    -------------------------
    # 1-12-2024             #
    -------------------------
*/


#include <gtk/gtk.h>  // GTK library for GUI
#include <stdio.h>     // Standard I/O functions
#include <stdlib.h>    // Standard library functions (e.g., atoi)
#include <string.h>    // String manipulation functions
#include <unistd.h>    // System calls
#include <sys/socket.h> // Socket programming functions
#include <netinet/in.h> // Defines for internet addresses
#include <arpa/inet.h>  // Functions for converting IP addresses

GtkWidget *progress_bar, *status_label, *ip_entry, *port_entry;
char selected_file[256] = "video_to_send_to_client.mp4"; // Default video file to send

// Function to handle video file transfer
void send_video(GtkWidget *widget, gpointer data) {
    const char *target_ip = gtk_entry_get_text(GTK_ENTRY(ip_entry));  // Get IP address from user input
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry)); // Get port number from user input
    int target_port = atoi(port_str); // Convert port string to integer

    // Check if IP or port is invalid
    if (strlen(target_ip) == 0 || target_port == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Invalid IP or port");  // Display error message
        return;
    }

    // Create a socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Socket creation failed");  // Display error message if socket creation fails
        return;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;  // Set address family to IPv4
    server_address.sin_port = htons(target_port);  // Set target port number
    inet_pton(AF_INET, target_ip, &server_address.sin_addr);  // Convert IP string to binary form

    // Attempt to connect to the server
    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Connection failed");  // Display error message if connection fails
        close(server_socket);  // Close socket
        return;
    }

    // Open the video file for reading
    FILE *fptr = fopen(selected_file, "rb");
    if (!fptr) {
        gtk_label_set_text(GTK_LABEL(status_label), "Failed to open file");  // Display error if file can't be opened
        close(server_socket);  // Close socket
        return;
    }

    fseek(fptr, 0, SEEK_END);  // Move the file pointer to the end of the file
    size_t file_size = ftell(fptr);  // Get the total file size
    fseek(fptr, 0, SEEK_SET);  // Move the file pointer back to the start
    send(server_socket, &file_size, sizeof(file_size), 0);  // Send file size to the server

    unsigned char buffer[1024];  // Buffer for reading the file
    size_t bytes_read, total_sent = 0;
    // Loop to read and send the file in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0) {
        if (send(server_socket, buffer, bytes_read, 0) < 0) {
            gtk_label_set_text(GTK_LABEL(status_label), "Error during file transfer");  // Display error message if sending fails
            fclose(fptr);  // Close the file
            close(server_socket);  // Close the socket
            return;
        }
        total_sent += bytes_read;

        // Calculate transfer progress
        double progress = (double)total_sent / file_size;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);  // Update progress bar

        // Update status label with the current progress percentage
        char status[64];
        snprintf(status, sizeof(status), "Transfer: %.2f%%", progress * 100);
        gtk_label_set_text(GTK_LABEL(status_label), status);

        // Process any pending GTK events to keep the UI responsive
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
    }

    fclose(fptr);  // Close the file after transfer is complete
    close(server_socket);  // Close the socket
    gtk_label_set_text(GTK_LABEL(status_label), "Transfer Complete!");  // Notify user that transfer is complete
    system("xdg-open .");  // Opens the target folder (optional)
}

// Callback function to handle file selection
void on_file_selected(GtkFileChooserButton *button, gpointer data) {
    const char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));  // Get the selected file name
    strncpy(selected_file, filename, sizeof(selected_file));  // Store the selected file
}

// Main function to set up the GUI and event handling
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);  // Initialize GTK

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);  // Create a new window
    gtk_window_set_title(GTK_WINDOW(window), "Video Sender");  // Set window title
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);  // Set window size

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);  // Create a vertical box container
    gtk_container_add(GTK_CONTAINER(window), vbox);  // Add the box to the window

    GtkWidget *file_chooser = gtk_file_chooser_button_new("Select Video File", GTK_FILE_CHOOSER_ACTION_OPEN);  // File chooser button
    g_signal_connect(file_chooser, "file-set", G_CALLBACK(on_file_selected), NULL);  // Connect file selection event
    gtk_box_pack_start(GTK_BOX(vbox), file_chooser, FALSE, FALSE, 5);  // Pack the file chooser into the box

    ip_entry = gtk_entry_new();  // Create an entry widget for the IP address
    gtk_entry_set_placeholder_text(GTK_ENTRY(ip_entry), "Enter Receiver IP");  // Set placeholder text
    gtk_box_pack_start(GTK_BOX(vbox), ip_entry, FALSE, FALSE, 5);  // Pack the IP entry into the box

    port_entry = gtk_entry_new();  // Create an entry widget for the port number
    gtk_entry_set_placeholder_text(GTK_ENTRY(port_entry), "Enter Port");  // Set placeholder text
    gtk_box_pack_start(GTK_BOX(vbox), port_entry, FALSE, FALSE, 5);  // Pack the port entry into the box

    progress_bar = gtk_progress_bar_new();  // Create a progress bar
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 5);  // Pack the progress bar into the box

    status_label = gtk_label_new("Ready to send");  // Create a label to display status
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 5);  // Pack the status label into the box

    GtkWidget *send_button = gtk_button_new_with_label("Send Video");  // Create a button to trigger video sending
    g_signal_connect(send_button, "clicked", G_CALLBACK(send_video), NULL);  // Connect the button click event to the send_video function
    gtk_box_pack_start(GTK_BOX(vbox), send_button, FALSE, FALSE, 5);  // Pack the send button into the box

    gtk_widget_show_all(window);  // Show all widgets in the window

    gtk_main();  // Start the GTK main loop

    return 0;  // Exit program
}
