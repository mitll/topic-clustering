#!/usr/bin/env python

import argparse
import os.path

from topic_id import TopicDataPackager

if __name__ == '__main__':

    arg_parser = argparse.ArgumentParser(description="Convert Kiva topic data to json")
    arg_parser.add_argument("data_dir", help="directory containing journal, loan, and topic lists")
    args = arg_parser.parse_args()
    data_dir = args.data_dir

    packager = TopicDataPackager()

    loan_ids = packager.read_id_list(os.path.join(data_dir, "loan_list.txt"))
    journal_ids = packager.read_id_list(os.path.join(data_dir, "journal_list.txt"))

    loan_10_topics = packager.read_topic_summary(
        os.path.join(data_dir, "summary_loan_10topics.txt"))
    loan_10_topic_probs = packager.read_topic_probabilities(
        os.path.join(data_dir, "topic_probability_loan_10topics.txt"))

    journal_10_topics = packager.read_topic_summary(
        os.path.join(data_dir, "summary_journal_10topics.txt"))
    journal_10_topic_probs = packager.read_topic_probabilities(
        os.path.join(data_dir, "topic_probability_journal_10topics.txt"))

    loan_50_topics = packager.read_topic_summary(
        os.path.join(data_dir, "summary_loan_50topics.txt"))
    loan_50_topic_probs = packager.read_topic_probabilities(
        os.path.join(data_dir, "topic_probability_loan_50topics.txt"))

    journal_50_topics = packager.read_topic_summary(
        os.path.join(data_dir, "summary_journal_50topics.txt"))
    journal_50_topic_probs = packager.read_topic_probabilities(
        os.path.join(data_dir, "topic_probability_journal_50topics.txt"))

    print("read everything")
    packager.process(document_id_list=loan_ids,
                     topic_list=loan_10_topics,
                     topic_probabilities=loan_10_topic_probs,
                     output_filename="loan_10_topics.json")

    packager.process(document_id_list=loan_ids,
                     topic_list=loan_50_topics,
                     topic_probabilities=loan_50_topic_probs,
                     output_filename="loan_50_topics.json")

    packager.process(document_id_list=journal_ids,
                     topic_list=journal_10_topics,
                     topic_probabilities=journal_10_topic_probs,
                     output_filename="journal_10_topics.json")

    packager.process(document_id_list=journal_ids,
                     topic_list=journal_50_topics,
                     topic_probabilities=journal_50_topic_probs,
                     output_filename="journal_50_topics.json")

